#include "pch.hpp"
#include "control.h"
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <fstream>


namespace nova { namespace redis {


namespace {


//String for update-rc.d to enable the redis-server daemon.
static const std::string REDIS_ENABLE = "sudo update-rc.d redis-server enable";

//String for update-rc.d to disable the redis-server daemon.
static const std::string REDIS_DISABLE = "sudo update-rc.d redis-server disable";

//String for stopping the redis-server daemon.
static const std::string REDIS_STOP = "sudo /etc/init.d/redis-server stop";

//String for starting the redis-server daemon.
static const std::string REDIS_START = "sudo /etc/init.d/redis-server start";


}//end anon namespace


/*
 * Constructor for the Control class.
 * std::string pid_file: string that is a path to the pid_file for redis.
 */
Control::Control(std::string pid_file) :
    _pid_file(pid_file)
{
    //Nothing in here.
}

/*
 * stop method for the Control class.
 * This stops the redis-server daemon.
 * First it grabs the PID from the redis PID file
 * Attempts to stop redis using init.d.
 * If that fails we run SIGNULL to see if the process is still alive.
 * Then we attempt to SIGTERM to gracefully clean up the redis-server process.
 * If that returns a Non Zero status code we SIGKILL the process.
 * This method returns an Int and will return a Non Zero
 * status code if we fail.
 */
int Control::stop()
{
    int pid;
    int status;
    if((status = system(REDIS_STOP.c_str())) == -1)
    {
        if((pid = _get_pid()) != -1)
        {
            if (kill(pid, 0) == 0)
            {
                if(kill(pid, SIGTERM) == 0 ||
                   kill(pid, SIGKILL) == 0)
                {
                    status = 0;
                }
            }
            else
            {
                status = 0;
            }
        }
    }
    if (status == 0)
    {
        unlink(_pid_file.c_str());
    }
    return status;
}

/*
 * start method of the Control class.
 * This method uses system to call init.d
 * to start the redis-server daemon.
 * It returns an int. Expect a Non zero status code on failure.
 */
int Control::start()
{
    return system(REDIS_START.c_str());
}

/*
 * disable method of the Control class.
 * This method uses system to call the 
 * update-rc.d command to disable the
 * redis-server daemon.
 * This method returns an int and will return 
 * a non zero status code on failure.
 */
int Control::disable()
{
    return system(REDIS_DISABLE.c_str());
}

/*
 * enable method of the Control class.
 * This method uses system to call the 
 * update-rc.d command to enable the
 * redis-server daemon.
 * This method returns an int and will return
 * a non zero status code on failure.
 */
int Control::enable()
{
    return system(REDIS_ENABLE.c_str());
}

/*
 * get_pid private method of the Control class.
 * This method attempts to get the pid from the redis-server
 * pid file.
 * This method opens the pid file.
 * Ensures that the file is open.
 * grabs the first line of the pid file
 * Checks to see if the length of the line is longer than zero.
 * Then does a boost::lexical_cast to change the resulting string into an int.
 * This method returns an int and will return a -1 
 * on error of locating the pid.
 */
int Control::_get_pid()
{
    std::string line = "";
    std::ifstream pfile(_pid_file.c_str());
    if (!pfile.is_open())
    {
        return -1;
    }
    std::getline(pfile, line);
    if (line.length() <= 0)
    {
        return -1;
    }
    return boost::lexical_cast<int>(line);
}


}}//end nova::redis namespace
