Welcome sneakers. This is a guest. This is only a guest.

Setting up Your Environment
===========================

You can build Sneaky Pete on your real machine, or on its own VM using the
included Vagrantfile, but the recommended (and currently used) method is to
build it inside the Trove Vagrant VM.

Vagrant:
http://vagrantup.com/

Sneaky Pete is currently set up to build for Debian Squeeze. Make sure your
VM is running that and run the file vagrant/initialize.sh to install
dependencies.


How to Build
============
This project is built using Boost Build v2, a build tool used to build the
Boost libraries. The command for this program is "bjam".

The script "vagrant/initialize.sh" will install Boost Build and other
dependencies needed by the build process.

Boost Build is driven by the file "Jamroot.jam" in the root directory. To invoke
Boost Build, type "bjam" at the command line from the source directory
(`/src` in the Vagrant VM).

To build the Sneaky Pete, cd into this directory and type the following:

.. code-block:: bash

    bjam -d+2 -q deploy

Then run it with

.. code-block:: bash

    bin/gcc-4.7/release/link-static/nova-guest
    bin/gcc-4.7/release/link-static/sneaky-pete-mysql
    bin/gcc-4.7/release/link-static/sneaky-pete-redis

"-d+2" is optional and tells Boost Build to print out every command it executes.
Boost Build will take awhile to start up, so if you keep getting compiler
errors in a file a good technique is to copy the command it executes and run
that instead until you can build the necessary target without errors.

"-q" is optional and tells Boost Build to stop after the first error. It's not
good to build it this way in a CI setup but when building locally it can
make life easier.

To build tooling that can be used for debugging and testing Sneaky Pete run:

bjam -d+2 -q tools

To build the debug versions, run "deploy-debug" and "tools-debug" respectively.


Understanding the Build File
============================

The Unit Rule
-------------
The file uses a custom unit rule. Each time this rule is invoked, it creates a
named logical target, specifies the source files to be included in the
resulting object file, the other object and library files the given source will
need to work when linked together, the unit test source code (if any) and the
object and library files the unit test might need.

To build a particular unit, give bjam the logical name on the command line. For
example:

bjam -d+2 u_nova_json

will build the object file for the json code and also build and run its unit
tests.

All of these unit tests run when the entire program is built.

Valgrind
--------
The unit tests currently use Boost Test executed with Valgrind to detect
memory leaks. These tests do not currently cause the build to fail if there is
a leak which is unfortunate. Valgrind also produces a large number of false
positives. Therefore its up to the maintainers right now to watch these results
carefully when updating the code.

The important thing is that "definitely lost," "indirectly lost", and
"possibly lost" all be 0 bytes. Sometimes "still reachable" may be more than
zero due to some libraries containing pointers to freed memory on program exit.


Functional Tests
----------------
In addition to the unit tests, there are functional tests which do not run
automatically as they require certain dependencies to be installed.

For example, the MySQL code needs to run against a real mysql database and
create tables. Forcing this to be present just to build the code could be
an impediment in some situations.

To run the functional tests, specify the target name when calling bjam. For
example:

bjam -d+2 send_and_receive

Will build the tests for AMQP functionality.

All of these functional tests are executed by the Reddwarf package scripts
mentioned above.


Testing in a real Trove Container
---------------------------------
The Trove VM should have a local apt repo and will probably already be
building and installing the Sneaky Pete agent when it creates an image used
to provision new Trove instances. So one way to test changes is to build the
agent and then build a brand new image. This takes forever though.

If you already have a running OpenVZ container, a quicker way to debug Sneaky
Pete is to install it into that container, shut down the running Sneaky Pete
process and run your new one.

To do this,

1. Create an instance using the Trove API.

2. Build Sneaky Pete as explained above.

2. Copy this entire directory (the one with the README file you're reading
right now) into it by executing the script "copy-to-guest.sh" which is in this
directory with the ID of the container as an argument. For example if the
container appears as container 1 when you execute "sudo vzlist", then:

/agent/copy-to-guest.sh 1

3. Enter the container with "sudo vzctl enter 1".

4. An instance of the agent will already be running. Stop it with "sudo service
sneaky-pete stop".

4. Login as the nova user (remember, Sneaky Pete needs to run as this user
so there's not much point in debugging as root).

su nova

5. The copy-to-guest.sh script will have place things inside the /agent
directory. Execute the newer version of SP with the following command:

/agent/deploy/sneaky-pete --flagfile=/etc/nova/guest.conf


A Note on Logging when Testing
------------------------------

The default flags for Sneaky Pete cause it to log to a file. If
you're debugging, it can be easier to log to stdout. This is achieved by
changing the flag value "--log_use_std_streams=true". You can also show even
more log data that usual by setting "--log_show_trace=true". Note that the
trace option will show all output from processes run by Sneaky Pete which
usually makes things more confusing.


Running RPC Methods Directly
----------------------------

Sometimes you might want to make Sneaky Pete run one RPC method. One way to do
this is to make the Trove code call Sneaky Pete the usual way and then see
what Sneaky Pete does.

However, its also possible to make Sneaky Pete start up and instantly run an
RPC method by setting the flag "--method", like so:

/agent/deploy/sneaky-pete --flagfile=/etc/nova/guest.conf '--message={"method":"create_user", "args": {"users":[{"_name":"blah", "_password":"poo", "_databases":[]}]}}'

This will run the method "create_user".

Another method that is sometimes necessary to debug is "prepare." This is called
usually as soon as Sneaky Pete starts up, and installs MySQL and then changes
settings in order for Sneaky Pete to be able to log in.

However, if MySQL is already installed, the call aborts. This is done on
purpose to ensure erroneous calls to "prepare" aren't being made, but it makes
it hard to debug. To get around it, set the flag "--skip_install_for_prepare",
like so:

/agent/deploy/sneaky-pete --flagfile=/etc/nova/guest.conf '--message={"method":"prepare", "args":{"memory_mb":512, "databases":[{"_name":"blah"}], "users":[] }}' --skip_install_for_prepare=true


Using a Debugger
----------------

If you're experiencing a crash in Sneaky Pete, running it in a debugger can
sometimes allow you to see the exact line number where the error is occurring.

1. You will need to build the debug version of Sneaky Pete. Just replace
"release" to "debug" when building Sneaky Pete:

bjam -d+2 -q link=static debug

2. Log into the container, and install gdb with:

sudo apt-get install gdb

3. Login as the nova user.

su nova

4. Execute the debug version of SP with the following command:

gdb /agent/deploy/sneaky-pete

7. Inside the gdb session, run it with the flag files via

run --flagfile=/agent/debian/guest.conf

Remember you can also pass in the "message" flag as explained above.

