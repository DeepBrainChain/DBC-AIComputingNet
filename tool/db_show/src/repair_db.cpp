#include "comm_def.h"

void repair_db(const char *path)
{
    leveldb::Options  options;

    options.create_if_missing = true;
    options.reuse_logs = true;
    try
    {
        // open 
        leveldb::Status status =  leveldb::RepairDB(path, options);
        if (false == status.ok())
        {
            cout << "repair failed";
            exit(0);
        }
        
       
    }
    catch (const std::exception & e)
    {
        cout << e.what();
    }
    catch (...)
    {
        cout << "eeor";
    }
}