/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include 	<signal.h>
#include	<sys/resource.h>	/* needed for wait */
#include	<errno.h>

#include	<sfio.h>

#include	"ningaui.h"
#include	"ng_ext.h"
#include	"ng_lib.h"
#include	"ng_tokenise.h"

#include	"nawab.h"
#include	"n_netif.c"			/* network interface stuff */
#include 	"nawab.man"


Symtab		*symtab = NULL;			/* reference to just about everything */
long		progid = 0;			/* programme id to keep variables scoped in the symbol table */
long		auto_progname=0;		/* unique id used when generating programme names for the user */
Sfio_t		*bleatf = NULL;			/* where we bellow when running as a daemon */
char		*mynode = NULL;			/* the name of the node that we are running on */
char		*version = "v3.8/0a149";
int		do_ckpt = 1;			/* turned off by -n  if no checkpointing is desired */
int		gflags = 0;			/* global flags -- GF_ constants */
char		*tumbler_vname = "NG_NAWAB_TUMBLER";	/* variable containing our ckpt file tumbler info -- allows for mult nawabs if reset */

void usage( )
{
	fprintf( stderr, "version: %s\n", version );
	fprintf(stderr, "Usage: %s [-c ckpt-file] [-C tumblers] [-f] [-n] [-p port] [-r recovery-file] [-v]\n", argv0);
	exit( 1 );
}

void logsignal( int s )
{
	ng_bleat( 0, "ignoring signal received: %d", s );
}

int main(int argc, char **argv)
{
	int	bg = 1;				/* set to 0 by -f option to keep us in the foreground */
	char	*port = NULL;
	char	*ckptfn = NULL;			/* checkpoint file name */
	char	*recovfn = NULL;		/* file that contains status messages received after last checkpoint was written */
	char	*cp;				/* general chr pointer */
	char	wbuf[256];			/* work buffer */
	char	*name;
	pid_t	ckpt_pid = 0;			/* id of pending checkpoint */

	char	*ckpt_tumblers = "0,0";		/* tumbler default setting for ng_ckpt_seed call */ /* deprecated */

	signal( SIGPIPE, logsignal );

	port = ng_env( "NG_NAWAB_PORT" );	/* get my listening port (default, options can override) */

	ARGBEGIN
	{
		case 'c':	SARG( ckptfn ); break;		/* checkpoint file name */
		case 'C':	SARG( ckpt_tumblers ); break;	/* the tumblers in low[,hi] order */
		case 'f':	bg = 0; break;
		case 'n':	do_ckpt = 0; break;		/* no checkpoint generation */
		case 'p':	SARG( port ); break;
		case 'r':	SARG( recovfn ); break;		/* recovery file name */
		case 't':	SARG( tumbler_vname );	break;	/* reset the tumbler info pinkpages variable name */
		case 'v':	OPT_INC( verbose ); break;
		default:
usage:
			usage( );
			exit(1);
	}
	ARGEND

	ng_sysname( wbuf, sizeof( wbuf ) );
	if( (cp = strchr( wbuf, '.' )) != NULL )
		*cp = 0;				/* make ningxx.research.att.com just ningxx */
	mynode = strdup( wbuf );			/* we assume we are running on the jobber */

	if( ! port )
	{
		ng_bleat( 0, "main: abort: cannot find NG_NAWAB_PORT, and -p port not given on cmd line" );
		usage( );
		exit( 1 );
	}

	ng_srand( );				/* seed lrand/drand */
	auto_progname = lrand48( );		/* get start of auto generated programme suffixes -- we incr this seq as we generate */
	symtab = syminit( 49999 );

	if( bg )
		hide( );

	ng_bleat( 1, "main: nawab %s: starting!", version );		/* version */

	/*ng_ckpt_seed( ckpt_tumblers, 50, 10 );*/

	if( ckptfn )
	{
		ng_bleat( 1, "attempting to read chkpt file: %s", ckptfn );
		if( ! ckpt_read( ckptfn ) )
		{
			ng_bleat( 0, "main: aborting! error(s) parsing checkpoint file: %s", ckptfn );
			exit( 2 );
		}

		if( recovfn )
		{
			ng_bleat( 1, "attempting to read recovery rollup file: %s", recovfn );
			if( ! ckpt_recover( recovfn ) )			/* roll up the jobs that were sussed from the checkpoint */
			{
				ng_bleat( 0, "main: aborting! unable to read recovery file [FAIL]" );
				exit( 1 );
			}
		}
		else
			ng_bleat( 1, "rollup file name was missing; no roll forward attempted.", recovfn );
	}
	else
	{
		if( recovfn )
		{
			ng_bleat( 0, "main: aborting! checkpoint file must be supplied when recovery file supplied" );
			exit( 3 );
		}
	}

	port_listener( port );				/* open network connections, and run, blocks until time to exit */

	if( verbose > 2 )
		dump_programmes( NULL, NULL );			/* dump them all */


	if( do_ckpt )
	{
		ng_bleat( 0, "shudown in progress, creating last ckpt file" );
		ckpt_pid = ckpt_asynch( "nawab" );		/* one last exhale -- will remove next one too */
		ckpt_wait( ckpt_pid );						/* wait on this one before exiting */

#ifdef OLD_SCHOOL
		name = ng_ckpt_peek( "nawab", 0 );			/* peek at the next one we will write -- erase it */
		ng_bleat( 1, "asynchronously removing next ckpt file: %s", name );
		sfsprintf( wbuf, sizeof( wbuf ), "ng_wrapper --detach ng_rm_register -u -f %s %s", verbose ? "-v" : "", name );	
		system( wbuf );
		ng_free( name );
#endif
	}

	ng_bleat( 0, "main: successful termination!" );
	exit(0);

	return 0;				/* keep compilers happy */
}

/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(nawab:Cycle Job Manager)

&space
&synop	ng_nawab [-c ckpt-file] [-C tumblers] [-f] [-n] [-p port] [-r recovery-file] [-v]

&space
&desc	&This is a daemon that accepts programmes via a TCP/IP session and expands the programmes read
	into a series of jobs that are sent to &lit(ng_seneschal) for execution.

	The following paragraphs describe the &this programming language:
&space
	A &this programme consists of one or more statements. Each statement may be one of the 
	following types:
&space
&begterms
&term	programme : Allows the user to name the programme.
&term	assignment : Allows the user to define variables and assign them values for reference 
	later in the programme. 
&term	job group : Describes a single unit of work that &this is to manage. 
&endterms

&space
&subcat	The Programme Statement
	The programme statement allows the user to assign a name to the programme being submitted 
	to &this. The programme name is used both to scope variables internally within &this, 	
	and is used as a part of the job name submitted to &ital(seneschal).  The use of the 
	programme name in a &ital(seneschal) job name allows the programme to be resubmitted to 
	&this without generating duplicate jobs in &ital(seneschal).  While the programme statement
	is not required, it is a good idea for a programme to have one. 
	The syntax for the programme statement is:
&space
	&lit(programme) &ital(name) [keep n][;]
.br
	&lit(programme;)
&space
	If &ital(name) ends with an underbar character (_), then &this will append a string to the 
	name in order to make the programme name unique.  If &ital(name) is completely omitted, 
	the semicolon is requried, and a unique programme name will be generated by &this.
&space
&subcat Programme Information Retention
	Unless specified with the &ital(keep) option on the &ital(programme) statement, information 
	about a programme is purged approximately one hour after the status of the last troupe 
	is receieived; if there were no errors.  If there were any errors, the programme information 
	is kept for three days (72 hours) following the last status message.  If a programme does not 
	complete, status for one or more jobs is never received, then &this will retain the programme 
	as an active programme indefinately.
&space
	The &lit(keep) directive on the &ital(programme) statment allows the user to define the amount 
	of time that &nawab will retain programme information after the last troupe has completed. The 
	value specified is assumed to be seconds, unless the value is less than 60. If the specified value
	is less than 60, then it is assumed to be days. 
&space
&subcat	Variables And Assignment Statements
	All references to a variable name, whether for assignment or expansion, are made using the syntax:
	&lit(%)&ital(name). Variable names must begin with an alphabetic character (a-z or A-Z)
	or an underbar character (_). The remainder of the name may contain alphabetic, numeric
	or underbar characters.  
	All alphabetic characters in a a variable name are case sensitive. Once set, a variable 
	retains its assigned value until reassigned or the end of the programme is reached. 
&space
	To assign a variable the equal (=) operator is used. The following illustrate several 
	variable assignments:
&space
	&lit(%_ATTEMPTS = 6) .br
	&lit(%DATE = "04/03/60") .br


&space	
&subcat	Defining a Job Group
	A job group is a collection of statements beginning with a job group name and followed 
	by one or more statements which define the job group.  Ultimately, a job group is converted
	into one or more jobs that are sent to seneschal for execution across the cluster. 

.**	The job statement is used to define a unit of work to &this. It provides a name, execution order
.**	information, execution target(s), iteration, input/output file names, and the command to 
.**	execute. The following is the syntax for a &this job statement:
&space
.**	&lit(name^: ^[order] ^[iteration] ^[comment] ^[consumption] ^[priority] ^[attempts] ^[node(s)] ^[inputs] ^[outputs] command)
&space

	The job group name follows the same rules as were described for variable names and must be 
	folowed by a colon.
	If the name is exactly &lit(job), then &ital(nawab) chooses a unique name (this is not recommended). 
&space
	Following the job group name, one or more of the following statments may be used
	to shape the job(s) associated with the group. 
	With the exception of the command statement, all other statements are optional.
&begterms
&term	order : If supplied the order information indicates which other job group(s) must completely 
	finish before commands created from this group are  sent to &ital(seneschal) for execution. 
	The syntax of the order statement is:
&space
	&lit(after) &ital(group-name[,group-name...])
&space
	If the order statment is omitted, the jobs generated will be sent to seneschal for 
	execution as soon as it is possible.  If a job group is blocked, &this will not send the 
	jobs to seneschal until all of the prereqresit jobs have successfully completed.
&space
&term	iteration : 
	An iteration statement is used to generate mulitple jobs for the job group.  
	One iteration statement is allowed per job group, and may cause a job to 
	be created for:
	
&indent
&beglist &ditext .75i
&item	mulitple nodes in the cluster
&item	every partition in an application's datastore
&item 	each record contained in a file
&item	each record generated by a command pipeline
&endlist
&uindent
&space
	Iteration claues are addressed in greater detail later.
&space

&term	comment : Is any string, enclosed in double quote marks (") that the user wishes to 
	assign to the job(s) generated by the group. 
&space
&term	consumption : The consumption clause allows the programmer to indicate which &ital(seneschal) resources
	are consumed by the jobs generated from this group.  The syntax for this clause is one of two forms: 
&space
	&lit(consumes [=] resource1[,resource2[,resourcen]]) 
&break	&lit(consumes [=] "resource1 [resource2[ resourcen]]")
&space
	&This passes the resource string directly to &ital(seneschal) without any modification. All jobs are given a
	resource of &lit(Rnawab) regardless of the presence or absence of the consumption clause.
&space
	Any of the resources may be given a default value that &ital(seneschal) will use as the resorce limit.  &ital(Seneschal)
	if the resource will use this default only if the limit was not set when these jobs are submitted. 
	When supplying a default value for a resource, 
	the resource name is suffixed with an &ital(=ns) or an &ital(=nh) where &ital(n) is the value.
	The &ital(s) and &ital(h) are constants indicating a hard (h) or soft (s) value. (See the seneschal 
	manual page for more information on hard and soft resource values and their effect on job execution).
&space
&term	priority : Is an expression of the form &lit(priority=)&ital(n) where &ital(n) is the priority that 
	is to be assigned to all &ital(seneschal) jobs created by this job group. Priorities values 
	are positive integers and must be less than 90.
	If this clause is omitted, then a priority of 40 is used as the default.
&space
&term	attempts : Allows the programmer to indicate the number of attempts that &ital(seneschal) should make 
	for jobs generated from this job statement before reporting a failure to &this. The syntax of the 
	cause is &lit(attempts=)&ital(n) where &ital(n) is the number of attempts. If this clause is omitted, 
	then three (3) attempts are requested. 
&space
&term	nodes : Defines limitations of where the jobs generated by the troupe are allowed to execute. The 
	values supplied on a &lit(nodes) statement may be one of:
&space
&beglist
&item	The node that the job(s) must be executed on. Jobs will block in seneschal until they can be executed 
	on the named node, even if the job(s) could be started on another node in the cluster.  
&space
&item	A node name preceded with a not symbol (e.g. !bat10) indicating that the command may be run 
	on any node except the one listed.
&space
&item	A group of node attributes (e.g. { !Saellite FreeBSD }).  Jobs may be started on any node that 
	has all of the listed attributes, and does not have any of the attributes that are listed with the 
	not (!) symbol.  Attributes must &stress(always) be listed as a group (enclosed in curly braces), 
	even if only one attiribute is indicated. 

&endlist
&space
&term	woomera : Allows woomera job information for the command to be supplied. Typically the data supplied 
	here are woomera resource names, but may be any information that precedes the &lit(cmd) token on a 
	woomera command. If the information supplied on this statement is more than a single word, the 
	string must be contained in dobule quotes.  The syntax for this statement is: &lit(woomera="string").

&space	
&term	required_inputs : This statement indicates the minimum number of input files that must reside on the 
	same node in ordr for a job to be started. By default, if this statement is not contained in the step, 
	all files must reside on the same node before the job will be started. 
&space
	When the required inputs value is supplied, the job can be started  when at least &lit(n) files 
	exist on a node.  If more than one node contains at least &lit(n) of the input files, then an attempt to select the 
	node with the most files will be made.  Node selection depends on other resource limits, and running jobs, so 
	it is very possible that the node that runs the job may have fewer of the input files than another node.  
	This option has enough sideeffects that the user should think hard before using it. 
&space
&term	inputs : 
	An input statement is required for each input file that the job requires.  Jobs are blocked by &ital(seneschal)
	until all input files reside on a common node.  When the job is started by &ital(seneschal,) each input file
	referenced on the command line is expanded to be the fully qualified pathname of the file in replication manager
	space (all input files specified this way must be registered with replication manager). 
	Only the file's basename should be supplied on an input statement. 
	The syntax of an input expression is:
&space
	&lit(<- %)&ital(vname) &lit(^[=) &ital(filename) &lit(^[:= {)&ital(gar-command)&lit(}]]) &lit([ignore_size!])
&space
	The variable must be assigned a value the first time it appears in an input statement, and after that can be 
	referenced as an input file in subsequent job groups. The variable name is then used as a command line parameter
	which will be expanded to supply the fully qualified pathname when the command is executed. 
	When the input expression takes the assigned form (%name = fname), an optional command may be provided following the 
	&lit(:=) "garred by" operator. 
	The command is passed to &ital(seneschal) who may use the command to generate the file if the file cannot be 
	found in the replication manager environment.
	The command may be a string, or a variable which references a string and may contain references to other 
	programme variables (e.g. := ng_addfactory -d %datastore -e %edition).
	&ital(Seneschal) and &this make several assumptions
	about the gar command and/or the programme that is invoked by the command:
&space
&beglist
&item	Any command options (e.g. edition number or datastore name) must be supplied as a part of the static command string.
&item	&ital(Seneschal) assumes that multiple files that indicate the same gar command can be created with one invocation of the command
	supplying each desired filename as a parameter on the command line. 
&item	The command will register all created files with the &ital(replication manager.)
&item	The command recognises when it is not possible to create a requested file. In this case it &stress(does not)
	create an empty file, nor does this cause the command to fail.
&endlist

&space
	If the &lit(ignore_size!) token is added as the last parameter of the statement, seneschal will be asked not
	to include the file's size in the job size computation. This is useful when  a number of jobs all reference a 
	common set of files; each of the common files should be given the ignore_case! directive. 


&space
&term	outputs : 
	Files that are generated by the command, and registered in replication manager space, must be defined as output 
	files in the job group.  The output file(s) should be passed into the command as a command line parameter.  
	When a command is executed, &ital(seneschal) will generate temporary basenames for each output file in order 
	to prevent duplication if a command is executed multiple times.  The command is responsible for creating 
	the file in replication manager space, and registering the file with replication manager when terminating 
	successfully.  &ital(Seneschal) is then responsible for causing the temporary name to be converted to the basename
	supplied for each output in the job group.
	The syntax for an output clause is:
&space
	&lit(-> %)&ital(vname) &lit(^[=) &ital(filename)&lit(])
&space
	The first use of the variable name must be assigned a filename value.  After its initial use, the variable name
	may be used as an input file to another job group, and when doing so needs no value. 
&space
&term	command : 
	The command statement defines the actual command that is executed for each job.  If an iteration clause is 
	supplied with the job group, then the command will be executed once for each target in the iteration. 
	Variables defined earlier, as well as input and output file variables, may be referenced on the command line. 
	The special variable &lit(%j) can be placed on the command line causing &ital(seneschal) to place the job name
	on the command line.  

&endterms	
&space	
	Older versions of &this required the job group statements to be in a very specific order. &This has been 
	modified such that the statements may be placed in any order between job statements.  

&space
&subcat Iteration Clause Specifics
	The iteration clause causes the genertion of multiple of jobs from a single job group specificaton
	as the iteration is evaluated.  The general syntax of an iteration clause is:
&space
&litblkb
    [%v < type(value)>]
&litblke
&space
Where:
&begterms
&term	v : Is the iteration variable name.  This variable may be used in the job group statements and evaluates to 
	the current itreation value (such as file or node name).
&space
&term	type : Is a constant character string from the set: contentsf, contentsp, partitions, partnums, or nodes.
&space
&term	value : Is data that is necessary to process the type.  Not all types require a value portion.
&endterms
&space
	The following are examples of iteration statements for each type which are explained in detail in 
	subesquent paragraphs:
&space
&litblkb
  [ %n in < nodes("command") > ]
  [ %p in <partitions(app_name)>]
  [ %pn in <partnums(app_name)> ]
  [ %f in <contentsf("/path/path/filename")> ]
  [ %pf in <contentsp("command[|command...]")> ]
  [ %x in < startv .. endv > ]
  [ %x in < startv .. endv "%03d" > ]
&litblke
&space
&subcat Node Iteration
	When the node iterator is specified, a job is created for each node in the cluster. The iteration 
	variable (%n in the above example) is expanded to be the node name and can be used on the command 
	line or as a part of a filename. 
	The command portion of the nodes iteration type is optional and if
	supplied causes the command to be executed which is exprected to supply the list of nodes to 
	execute the job on. 
	The command is expected to generate the list on 
	standard output as a single, newline terminated, record.  The tokens on the line may be comma or
	blank separated. The following statements would cause jobs to be generated for all FreeBSD nodes
	in the cluster, on a set of three specific nodes, and all nodes except those with the Sattelite 
	attribute:
&space
&litblkb
   [ %n in <nodes( "ng_species FreeBSD" ) > ]
   [ %n in <nodes( "echo ant00,ant01,ant06" ) > ]
   [ %n in <nodes( "ng_species !Satellite" ) > ]
&litblke
&space
	The nodes iteration is &stress(very different) than the &lit(nodes) statement as it causes
	a job for each node in the list, rather than listing a node or attribute as the target 
	for a single job. 

&subcat Partition Iteration
	The partition iterator causes a job to be created for each partition known to be in the datastore 
	of the named application. The number of partitions for an application is assumed to be defined 
	in the cartulary using the variable name &lit(NG_)&ital(app-name)&lit(_PARTS).  In addition to 
	the iteration variable being set for each iteration, a special variable, &lit(Pnum) is set for 
	each iteration.  The value is set to the numeric portion partition name.

&space
&subcat Partition Number Iteration
	Similar to the partition iteration, the partition number creates a job for each partition thought 
	to be in the datastore for an application.  The iteration variable is set to the partition number,
	(0 - NG_xxx_PARTS-1).  

&space
&subcat Range Iteration
	Iteration over a range of numbers is possible using the &lit(start..end) construct. Depending on 
	where the range iterator is placed (on an file directive, or at the step level) causes the 
	appropriate files/steps to be generated assigning the variable an integer value beginning with 
	&ital(start) and ending with &ital(end) (inclusive). 
&space
	A format of "%05d" is used by default when creating the contents of the variable supplied. An 
	optional format string (of the form %[0]&ital(n)d) can be supplied between the end specification and 
	the terminating angle bracket (>). The following illustrates how to set a format that causes the 
	variable (p) to be set with leading zeros padded to 3 characters:
&space
&litblkb
	group_name : [ %p in <0 .. 999 "%03d" > ]
&litblke

&space
&subcat File Content and Command Pipeline Iteration
	The contents of a file, or  the output of a command pipeline, can be used as iterations when 
	creating a series of jobs. In both cases &this reads records from the pipe/file until the end
	is reached. A job is created for each newline terminated record that is read.  
	Records contain blank separated tokens that 
	can be referenced using the iteration variable with a token number appended (e.g. %f1). The following
	illustrates how the three tokens from the named file can be used to generate a command that needs
	two input files and produces a third output file:
&space
&litblkb
 	job_group_0:
                [ %f in <contentsf(/home/ningaui/src/cycle/regression/data2)>]
                comment "munge the data/generate update records"
                consumes Rdaniels
                priority = 40
                attempts  1
                <- %inputa = %f1
                <- %inputb = %f2
                -> %update = %f3
                cmd dowork %inputa %inputb %update
&litblke
&space
&subcat Iteration Shortcut
	An iteration 'shortcut' can be taken if a job is to be executed using the same iteration defined 
	in the previous job statement.  This shortcut has the syntax:
&space
	&lit(^[%)&ital(iname)&lit(])
&space
	With &ital(iname) being the &stress(same) iteration variable name defined in the previous job statement.
	A second job group following the previous example in the same programme, might be defined this way
	so as the output file created by each of the previous jobs is used as the input file to the next 
	set of jobs:
&space
&litblkb
       job_group_1:
          [ %f ]   # use iteration defined in _0
          "generate analysis for beths group"
          priority = 30
          <- %update	# from previous step
          -> %rpt = rpt-%today-%f3
          cmd rpt_gen -i %update -o %rpt
&litblke
&space
	This job group is tricky in that it uses the iteration as defined in the previous step, as well as 
	using the variable name &lit(%update) as defined in the previous step.  

&space
&subcat Variables In Iterators
	Variable names may be used in iteration statements; for example:
&space
&litblkb
      [ %f in <contentsf( %MCD )>]
      [ %f in <contentsp( "%bindir/command_x" )>] 		
&litblke

&space
&subcat	Example Programme
	The following is a small &this programme that contains four job groups. 
	The first group creates a series of files. Commands generated by the second group are executed when the appropriate
	file becomes available, and creates a file that is used by the third group. As with the second job's commands,
	the commands created by third job will block until their input files are ready. The fourth job is set to 
	execute after all of the commands from the second and third jobs have completed successfully and it does 
	cleanup work.  Because the last group does not rely on specific input files, it uses the &lit(after) 
	clause to block its execution until all jobs from the named group have succesfully completed. 
&space
&litblkb
	programme simple5 keep 4;
	%nfiles = 1000		# number of files to create - numbered 0 to n-1
	%prefix = scooter	# step1 job creates files with this prefix
	%bin = /home/daniels/bin

	step1:	comment "create the input files for step2, pass %nfiles and %prefix as parameters"
		consumes Rcycle,Rs1=2n	# max 2/node
		priority = 65
		nodes ning02
		cmd	%bin/create_files %nfiles %prefix

	step2:	[%p in < 1 .. %nfiles >] 
		comment "jobs should wait for their input and then create their output"
		consumes Rcycle,Rs2=1n
		priority = 55
		attempts = 4
		<-	%s2_in = %{prefix}_%p
		->	%s2_out = step2_output.%p 
		cmd %bin/use_input %s2_in %s2_out
	
	step3:	[%p]	
		comment "jobs cleanup what was created in step 2 (using same iterator)"
		consumes Rcycle
		priority = 54
		woomera = "Cleanup"
		<-	%s2_out
		cmd %bin/clean_up %s2_out
	
	step4:	after step3	
		comment "get a dump of files that were not cleaned up properly"
		consumes Rcycle
		attempts = 5
		nodes { ! Satellite }
		cmd %bin/report_problems
&litblke
&space

&subcat Programme Submission
	The &this interface programme, &lit(ng_nreq,) is used to submit programmes to &this, and 
	to issue commands for &this to act upon.  The syntax to submit (load) a programme that has 
	been saved in &lit(/tmp/n.pgm) is:
&space
&litblkb
	ng_nreq [-v] -l </tmp/n.pgm
&litblke
&space
	When &lit(ng_nreq) is invoked to submit a programme, and the verbose option is indicated, 
	the programme submission status is written to the standard output device. The exit status
	of &lit(ng_nreq) indicates success of failure regarding the submission and parsing of the 
	programme.

&subcat Commands
	Using the &lit(ng_nreq) interface to &this, one of several commands may be sent to &this
	in order to control &this as it is executing. The general syntax to send a command 
	is:
&space
&litblkb
	ng_nreq command-name [parms]
&litblke
&space

	When a command is received, the command is processed, and any resulting output is 
	returned to &lit(ng_nreq) which writes the received information to the standard
	output device. 
	The following is a list of commands recognised by &this:
&space
&begterms
&term	dump : causes information about a programme, or a specific set of jobs (troupe), or
	a programme's variables to be written to the standard output device. The command 
	syntax has three forms which are:
&space
	&lit(dump prog) &ital(programme-name) 
&break	&lit(dump troupe) &ital(programme-name) &ital(troupe-name)
&break	&lit(dump variables) &ital(programme-name) 

&space
&term	explain : Similar to the dump command, the explain command writes information about 
	a programme and optionally a troupe to the standard output device. The information 
	is not quite as detailed as the information produced by the dump command and is 
	aimed to assist in determining the status of a programme rather than debugging
	&this. The explain command accepts one of three parameters:
&begterms
&term	all : Causes all programmes to be explained.  One line per programme is written
	followed by a line for each troupe within the programme.  Information for each 
	failed job is also listed.  If the explain parameter is omitted, all is assumed.
	This can generate a significant amount of output.
&space
&term	prog : Causes only the programme information for each programme known to &ital(nawab)
	to be written.  Troupe and failure information is supressed.
&space
&term	programme-name : If a known programme name is supplied, then all information about 
	the programme, its troupes, and failures is written.
&endterms

&space
&term	purge : The purge command causes &this to remove all information about the programme 
	that is named as the command's only argument. When a programme is purged from 
	&this, a purge request is sent to &ital(seneschal) for each job that had previously
	been submitted.

&space
&term	verbose : Allows the verbose setting to be adjusted. The verbose command must be 
	followed by a number between 0 and 10.  Following receipt of the verbose command,
	&this will begin to scribble its verbose messages to its private log using the 
	new level.  A message is also written to the log to indicate that the level 
	was altered.
&endterms
&space



&opts	The following options are recognised by &this when placed on the command line:
&begterms
&term	-C lo^[,hi] : Supplies checkpoint tumbler numbers.
&space
&term	-c filename : Supplies the checkpoint file that should be read upon restart.
&space
&term	-f : Do not execute as a daemon; remain in the "foreground" writing verbose messages 
	to the standard error device rather than a private log file.
&space
&term	-n : No checkpoint.  Checkpoint files will not be generated.
&space
&term	-p port : Defines the port number that &this should listen for TCP and UDP messages on.
	If this option is not defined, &this expects that the cartulary variable &lit(NG_NAWAB_PORT)
	will be set to the port number it should use. 
&space
&term	-r filename : Provides the name of the file containing recovery information that should 
	be applied after the checkpoint file is read.  Recovery information is in the form of 
	status messages that would have been sent from &ital(seneschal.)
&space
&term	-t vname : Defines the pinkpages variable that will be used to track checkpoint tumbler 
	information.  If not supplied, NG_NAWAB_TUMBLER is used.  If multiple copies of 
	&this are to be run where checkpoint files would overlap in replicaiton manger space, 
	each additional copy MUST use different tumbler information and thus each must have
	its own pinkpages variable. 
&space
&term	-v : Verbose mode. Several can be placed on the command line to increase the noise
	created by &this. If verbose level 5 is set, then YACC debugging is turned on as well.
&endterms


&space
&files	When &this is executed as a daemon, the working directory is changed to &lit($NG_ROOT/log)
	and a  private log file (&lit(nawab.log)) is opened and used to log verbose messages.

&space
&exit	&This is designed to execute as a daemon and should end only when asked or if there is 
	an error.  The log file should indicate why it stopped.

&space
&bugs
	The filename supplied in a file iterator clause must be local to the node that is running
	&this.  Eventually this should be changed such that it is a basname of a file located in 
	replication manager space and potentially available to &this regardless of its location.

&space
&see	ng_nreq, ng_seneschal, ng_sreq

&space
&mods
&owner(Scott Daniels)
&lgterms
&term 	14 Jan 2002 (sd) : Enhancement for communications interface and job management.
&term	24 Feb 2003 (sd) : Converted chkpt file seperator to be tilda rather than bang -- 
	it was causing issues with !leader jobs.
&term	20 Mar 2003 (sd) : Modified the delete of the next checkpoint file such that it is 
	done asynch because we really dont need to know the status.
&term	11 Apr 2003 (sd) : Corrected issue with programme name.
&term	20 Apr 2003 (sd) : Added the ability to generate a set of jobs based on records from a file containing 
	file (input and/or output) information.
&term	22 Apr 2003 (sd) : Changes to file/pipe iterator as suggested; enhanced man page.
&term	05 Jul 2003 (sd) : relinked with new ng_log function to avoid broadcast issues. bumbed version
	number to 1.4.
&term	05 Oct 2003 (sd) : To ensure sf files are closed during detach process. 
&term	06 Nov 2003 (sd) : Added format ability to n..m iteration.
&term	04 Dec 2003 (sd) : Added support for node attributes (version bumped to 1.6).
&term	08 Dec 2003 (sd) : Added woomera statement to job description.
&term	12 Jan 2004 (sd) : Added better error message diagnostics to user interface.  Now indicates the 
	auto generated programme name if user did not supply, or supplied name_.
&term	04 Feb 2004 (sd) : Added errors if parsing descriptors (pass 2) fails.
&term	13 Feb 2004 (sd) : Reworked the auto programme name generation -- removed use of timestamp.
&term	23 Feb 2004 (sd) : Added the programme submission time to the explain output, and added a 
	'prog' subcommand to explain to list just the programme info and not troupe info.
&term	24 Mar 2004 (sd) : Added paralell checkpointing. (version now 1.8)
&term	06 Apr 2004 (sd) : Corrected bug with creation of checkpoint filename at shutdown.
&term	26 Apr 2004 (sd) : Now returns an error to user if an invalid command is received via UDP. 
&term	11 Jun 2004 (sd) : Added ability to set max per node on a consumes statement (x=3n).
		Job submission now pauses every now and again to allow user commands and job status 
		messages to be processed. Now saves submit time of programme in chkpoint file. (v1.9)
&term	26 Jun 2004 (sd) : Fixed bug in how we mark end of sysname. 
&term	13 Jul 2004 (sd) : Fixed bug in chkpt read -- not initialising jp block; bug in uif causing 
	a core dump if purge command had no arguments.
&term	18 Aug 2004 (sd) : Converted the portlistener to manage all by invoking ng_sipoll() rather than
	ng_siwait() which allowed the removal of the alarm() dependancy when not running with threads. 
	Prevents the occasional hang that could be cleared by sending an alarm signal. (v2.0)
&term	20 Aug 2004 (sd) : Corrected buffer overrun in the construction of the resource=value string in nawabgram.yy. (v2.1)
&term	22 Aug 2004 (sd) : Ensured that alarms are turned off as they are not needed.
&term	04 Jan 2006 (sd) : Added support for required inputs statement (v2.2)
&term	09 Jan 2006 (sd) : Crrected bug in grammer that was causing coredump. 
&term	25 Jan 2006 (sd) : Modified man page. 
&term	27 Jan 2006 (sd) : When running as a daemon, we no longer use sfclose() to on stdin/out/err. 
&term	28 Mar 2006 (sd) : fixed bug in fiter() that was preventing iteration variables from being deleted from the 
		symbol table and thus causing issues when creating a checkpoint file. 
		Grammar tweeks to allow ("command") on <node> iterator, eliminate YACC shift/reduce errors, 
		and to allow for unrestricted ordering of job statements.  Variable names can be used inside of
		iterations to supply filenames, commands, and hi/lo values.
&term	13 Apr 2006 (sd) : Purging a job now purges jobs from seheschal's complete queue preventing false status
		messages from being returned if the user submits the a programme with the same name immediatly
		after a purge. (v3.0)
&term	20 Apr 2006 (sd) : Fixed bug in the lo..hi iteration processing.
&term	12 Jun 2006 (sd) : Converted to use ng_tumbler.
&term	18 Jan 2007 (sd) : Added support for passing of user name from nreq when submitting a programme.
		Also added keep directive on programme line. (v3.1)
&term	27 Mar 2007 (sd) : Corrected issue with the way subuser and subnode strings were allocated. (v3.2)
&term	04 May 2007 (sd) : Corrected problem that caused core dump when no troupe (job group) name is given in the programme. (v3.3)
&term	19 Sep 2007 (sd) : We now capture the md5 value of the checksum file for checkpoint finding. The md5 is 
		written on the masterlog message that is generated after the file is successfully registered. (v3.4)
&term	19 Oct 2007 (sd) : Added a force to checkpoint as soon as a new programme is parsed.
&term	29 Nov 2007 (sd) : Changed timing on checkpoint creation to put it back to every 5 minutes. On systems where a new programme
		is submitted every few seconds we were overrunning repmgr's ability to keep checkpoint files from becomming  (v3.5)
		unattached.
&term	07 Dec 2007 (sd) : Added support for ignore_size! directive on input file statements. (v3.6)
&term	15 Sep 2009 (sd) : Added check to determine if error reading recovery file happened. (v3.7)
&term	14 Nov 2009 (sd) : Corrected call to bleat in purge_programme() to prevent coredump. (v3.8)
&endterms

&scd_end
------------------------------------------------------------------ */
