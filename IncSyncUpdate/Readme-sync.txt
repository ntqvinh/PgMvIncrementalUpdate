
The tool can generate all triggers in C-language for all data-changing events on all tables underlying upon query that creates the materialized view. The generated triggers do synchronous incremental updates for MV. Although the feature of synchronous incremental update integrated into the PostgreSQL source code may be more optimal plan. But the solution with triggers may have its benefit because of its relative independence from versions of the DBMS.

The tool is written in C in Windows environment. We're trying to adapt to the Linux environment, but we can not finish in the short time.

The matviews can be created by any queries with restrictions:
- inner join;
- aggregate functions: COUNT, CUM, AVG, MIN, MAX.
- no recursive;
- no having;
- no sub-queries;

Unfortunately in PostgreSQL triggers for statement can not hold the changing/changed records. 

The current version of program can work only with 32bit PostgreSQL, but the generated triggers can be built for both 32bit and 64bit versions, depending on the version of libs are included during compiling. The program is not implemented all of our algorithm. We have to do also some optimization. 

The user can find the run.bat in the release\example. o11dw-OK4-lowercase.backup file is the backup of the database transformed from Oracle 11g sample database. The query accompanied with the example is designed for that database. It requests the local PostgreSQL instance running at port 5432.

We use Visual Studio 2015 for building the generator. You can change the configuration as you want related to the platform, including [C/C++ -> General -> Additional Include Directories], [Linker -> General -> Additional Library Directories] folders and [Linker -> Input -> Additional Dependencies]. Please, don't forget to install Visual Leak Detector and set the project configuration for it too.

