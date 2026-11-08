#!/bin/bash

if [ -n "$DOCKER_ORACLE" ]; then
    echo "LISTENER = (ADDRESS = (PROTOCOL = TCP)(HOST = oracle)(PORT = 1521))" >> /usr/lib/oracle/tnsnames.ora
    echo "oracle =
(DESCRIPTION =
  (ADDRESS_LIST =
    (ADDRESS = (PROTOCOL = TCP)(HOST = oracle)(PORT = 1521))
  )
  (CONNECT_DATA =
    (UR=A)
    (SERVICE_NAME = ${ORACLE_PDB})
  )
)" >> /usr/lib/oracle/tnsnames.ora

    # setup environment
    echo export ORACLE_SID=qoresid >> /tmp/env.sh
    echo export ORACLE_PDB=QOREPDB >> /tmp/env.sh
    echo export ORACLE_PWD=qore >> /tmp/env.sh
    # a user that may create and drop users, with the privileges that OracleTestIsolation.qm grants the user of each
    # test run (examples/test/qlib/SqlUtil): SYSTEM (ORACLE_PWD is the password of SYS, SYSTEM and PDBADMIN; PDBADMIN
    # has only the PDB_DBA role, which need not have them)
    echo export QORE_DB_CONNSTR_ORACLE_SYS=oracle:system/qore@oracle >> /tmp/env.sh
    echo export QORE_DB_CONNSTR_ORACLE=oracle:qore/qore@oracle >> /tmp/env.sh

    . /tmp/env.sh
    set +e
    waited=0
    echo "Waiting for Oracle DB to start."
    while true; do
        out=`qore -ne "Datasource d(\"${QORE_DB_CONNSTR_ORACLE_SYS}\"); auto x=d.getServerVersion();" 2>&1`
        status=$?
        if [ "$status" = "0" ]; then
            echo && echo "Oracle DB started."
            echo "Waiting 10 seconds to let startup scripts prepare the test user and tablespaces."
            sleep 10 && break
        elif [ $waited -eq 60 ]; then
            echo && echo "Waited too long for Oracle DB to start. Aborting build."
            exit 1
        elif ! echo "$out" | grep -E "QoreOracleConnection::logon.*ORA-(01033|12514)" > /dev/null 2>&1; then
            echo "Error in Oracle initialization:"
            echo "$out"
            exit 1
        fi

        printf "."
        sleep 10
        waited=$((waited+1))
    done
else
    # no Oracle container: the Oracle tests are skipped
    echo "no Oracle container (DOCKER_ORACLE is not set); Oracle tests will be skipped"
fi
set -e
