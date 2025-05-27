#include <exception>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

import resurrect;
import utils.logger;



using namespace gpsxre;



int main(int argc, char *argv[])
{
    int exit_code = 0;
    std::filesystem::path file;
    bool recursive = false;
    bool force = false;

#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    try
    {
        for(int i = 1; i < argc; ++i)
        {
            std::string arg(argv[i]);

            if(arg == "--recursive" || arg == "-r")
                recursive = true;
            else if(arg == "--force" || arg == "-f")
                force = true;
            else if(file.empty())
            {
                file = std::filesystem::path(arg);
            }
        }

        if(file.empty())
        {
            LOG("usage: resurrect [options] <skeleton/hash>");
            LOG("");
            LOG("\t--recursive,-r         \tlook for files recursively");
            LOG("\t--force,-f             \tforce rebuild (skip missing files)");
            LOG("");
            LOG("Note: .skeleton and .hash must have same filename");
            exit_code = -1;
        }
        else
            exit_code = resurrect(file, force, recursive);
    }
    catch(const std::exception &e)
    {
        LOG("error: {}", e.what());
        exit_code = -1;
    }
    catch(...)
    {
        LOG("error: unhandled exception");
        exit_code = -2;
    }

    return exit_code;
}
