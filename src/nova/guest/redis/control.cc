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
Control::Control()
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
        if((pid = get_pid()) != -1)
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
 * get_pid method of the Control class.
 * This method attempts to get the pid from the redis-server
 * using popen and the pidof command.
 * Checks to see if the length of the line is longer than zero.
 * Then does a boost::lexical_cast to change the resulting string into an int.
 * This method returns an int and will return a -1
 * on error of locating the pid.
 */
int Control::get_pid()
{
    FILE * fd;
    char data[11] = "";
    fd = popen("pidof redis-server", "r");
    if (!fd)
    {
        return -1;
    }
    fgets(data, 20, fd);
    pclose(fd);
    std::string pid(data);
    if (pid.length() <= 0)
    {
        return -1;
    }
    return boost::lexical_cast<int>(data);
}

/*
 * get_process_status method of the Control class.
 * This method attempts to see if the redis-server process is running.
 * This method opens the pid file.
 * Checks to see if the pid file is not -1.
 * Then if it is not -1 it will SIGNULL the process to see if it is running.
 * If it is not we return -1 if it is running we return 0.
 */
int Control::get_process_status()
{
    int pid;
    pid = get_pid();
    if (pid == -1)
    {
        return -1;
    }
    if (kill(pid, 0) == 0)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}


}}//end nova::redis namespace
