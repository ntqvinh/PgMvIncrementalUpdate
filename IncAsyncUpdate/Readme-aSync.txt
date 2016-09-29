The project consists of 2 tools:

All the tool is written in C in Windows environment. We're trying to adapt to the Linux environment, but we can not finish in the short time.

1) Trigger generator: The tool can generate all triggers in C-language for all data-changing events on all tables underlying upon query that creates the materialized view. The generated triggers do collecting all the changes and saving into the tables. Although the feature integrated into the PostgreSQL source code may be more optimal plan. But the solution with triggers may have its benefit because of its relative independence from versions of the DBMS.

2) Updater: This tool generates source code in C-language 
The matviews can be created by any queries with restrictions:
- inner join;
- aggregate functions: COUNT, CUM, AVG, MIN, MAX.
- no recursive;
- no having;
- no sub-queries;

The generated codes do reading the collected changes and do the incremental update. It does the optimization that called "condense operator" with hoping that all the changes according to one record are collected in the order of transaction commits.

The current version of program is tested only with 32bit PostgreSQL, but the generated triggers can be built for both 32bit and 64bit versions, depending on the version of libs are included during compiling.  

We use Visual Studio 2015 for building the tools. The users can change the configuration as you want related to the platform, including [C/C++ -> General -> Additional Include Directories], [Linker -> General -> Additional Library Directories] folders and [Linker -> Input -> Additional Dependencies]. Please, don't forget to install Visual Leak Detector and set the project configuration for it too.

