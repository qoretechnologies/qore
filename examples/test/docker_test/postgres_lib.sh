#!/bin/sh

start_postgres() {
    docker run --name=postgres --network=host -e POSTGRES_PASSWORD=omq -e TZ=Europe/Prague -e PGTZ=Europe/Prague -d postgres:15

    # wait for PostgreSQL server to start
    printf "waiting on PostgreSQL server: "
    waited=0
    while true; do
        ver=`qore -ne 'try { printf("%s", (new Datasource("pgsql:postgres/omq@postgres%localhost:5432")).getServerVersion()); } catch () {}'`
        if [ -n "$ver" ]; then
            echo ": started server version $ver"
            break
        fi

        # 30 second time limit
        if [ $waited -eq 30 ]; then
            echo && echo "Waited too long for PostgreSQL to start; aborting build."
            exit 1
        fi
        printf .
        # sleep for 1 second
        sleep 1
        waited=$((waited+1))
    done

    export OMQ_DB_USER=postgres
    export OMQ_DB_PASS=omq
    export OMQ_DB_NAME=postgres
    export OMQ_DB_HOST=`qore -ne 'printf("%s", (map $1.address, get_netif_list(), $1.family == AF_INET && $1.address !~ /^127/ && $1.address !~ /\.0$/)[0]);'`
    export QORE_DB_CONNSTR_PGSQL=pgsql:${OMQ_DB_USER}/${OMQ_DB_PASS}@${OMQ_DB_NAME}%${OMQ_DB_HOST}

    # make sure we can access the DB
    qore -nX "(new Datasource(\"${QORE_DB_CONNSTR_PGSQL}\")).getServerVersion()"
}

setup_postgres_on_host() {
    # add env vars to environment file and load it
    # NOTE: must convert to lower case only, or the psql commands below will fail
    user=omq_test_`qore -lUtil -ne 'printf("%s", get_random_string(10));' | tr A-Z a-z`
    echo export OMQ_DB_USER=${user} >> /tmp/env.sh
    echo export OMQ_DB_PASS=omq >> /tmp/env.sh
    echo export OMQ_DB_NAME=${user} >> /tmp/env.sh
    echo export OMQ_DB_HOST=${RUNNER_HOST:=pgsql} >> /tmp/env.sh
    systemdb=pgsql:${user}/omq@${user}%${RUNNER_HOST:=pgsql}
    echo export QORE_DB_CONNSTR_PGSQL=${systemdb} >> /tmp/env.sh
    echo export QORE_DB_CONNSTR=${systemdb} >> /tmp/env.sh

    # if we have a password for the local postgres user, then use it and connect to localhost as well
    if [ -n "${POSTGRES_PASSWORD_MAP}" ]; then
        PGPASSWORD=`qore -l Util -ne "printf(\"%s\", parse_to_qore_value(\"${POSTGRES_PASSWORD_MAP}\"){\"${RUNNER_HOST:=pgsql}\"});"`
        if [ -n "${PGPASSWORD}" ]; then
            echo export PGPASSWORD=${PGPASSWORD} >> /tmp/env.sh
            echo export PSQL_ARGS=\"-h ${RUNNER_HOST:=pgsql}\" >> /tmp/env.sh
        fi
    fi

    . /tmp/env.sh

    # create user for test
    cat <<EOF | psql -Upostgres ${PSQL_ARGS}
create database ${OMQ_DB_NAME} encoding = 'utf8';
\connect ${OMQ_DB_NAME};
create user ${OMQ_DB_USER} password 'omq';
grant create, connect, temp on database ${OMQ_DB_NAME} to ${OMQ_DB_USER};
grant create on tablespace omq_data to ${OMQ_DB_USER};
grant create on tablespace omq_index to ${OMQ_DB_USER};
grant select on all tables in schema pg_catalog to ${OMQ_DB_USER};
grant all on schema public to ${OMQ_DB_USER};
EOF
    echo created pgsql user ${OMQ_DB_USER} and db ${OMQ_DB_NAME}

    # make sure we can access the DB
    qore -nX "(new Datasource(\"${QORE_DB_CONNSTR_PGSQL}\")).getServerVersion()"

    export POSTGRES_HOST=1
}

cleanup_postgres_on_host() {
    if [ "${POSTGRES_HOST}" = "1" ]; then
        # drop postgresql test user
        cat <<EOF | psql -Upostgres ${PSQL_ARGS}
drop owned by ${OMQ_DB_USER};
drop database ${OMQ_DB_NAME} with (force);
drop role ${OMQ_DB_USER};
EOF
        echo dropped pgsql user ${OMQ_DB_USER}
    fi
}

