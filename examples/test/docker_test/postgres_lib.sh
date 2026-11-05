#!/bin/sh

start_postgres() {
    docker run --name=postgres --network=host -e POSTGRES_PASSWORD=qore -e TZ=Europe/Prague -e PGTZ=Europe/Prague -d pgvector/pgvector:pg18

    # wait for PostgreSQL server to start
    printf "waiting on PostgreSQL server: "
    waited=0
    while true; do
        ver=`qore -ne 'try { printf("%s", (new Datasource("pgsql:postgres/qore@postgres%localhost:5432")).getServerVersion()); } catch () {}'`
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

    export QORE_DB_USER=postgres
    export QORE_DB_PASS=qore
    export QORE_DB_NAME=postgres
    export QORE_DB_HOST=`qore -ne 'printf("%s", (map $1.address, get_netif_list(), $1.family == AF_INET && $1.address !~ /^127/ && $1.address !~ /\.0$/)[0]);'`
    export QORE_DB_CONNSTR_PGSQL=pgsql:${QORE_DB_USER}/${QORE_DB_PASS}@${QORE_DB_NAME}%${QORE_DB_HOST}

    # make sure we can access the DB
    qore -nX "(new Datasource(\"${QORE_DB_CONNSTR_PGSQL}\")).getServerVersion()"

    # enable pgvector extension if available
    psql -Upostgres -h ${QORE_DB_HOST} -c "create extension if not exists vector;" ${QORE_DB_NAME} 2>/dev/null || true
}

setup_postgres_on_host() {
    # add env vars to environment file and load it
    # NOTE: must convert to lower case only, or the psql commands below will fail
    user=qore_test_`qore -lUtil -ne 'printf("%s", get_random_string(10));' | tr A-Z a-z`
    echo export QORE_DB_USER=${user} >> /tmp/env.sh
    echo export QORE_DB_PASS=qore >> /tmp/env.sh
    echo export QORE_DB_NAME=${user} >> /tmp/env.sh
    echo export QORE_DB_HOST=${RUNNER_HOST:=pgsql} >> /tmp/env.sh
    systemdb=pgsql:${user}/qore@${user}%${RUNNER_HOST:=pgsql}
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

    # create user for test; CREATEROLE lets SqlUtil's PgsqlBoundedTransaction.qtest create (and drop) the role its
    # row-security case needs
    cat <<EOF | psql -Upostgres ${PSQL_ARGS}
create database ${QORE_DB_NAME} encoding = 'utf8';
\connect ${QORE_DB_NAME};
create extension if not exists vector;
create user ${QORE_DB_USER} password 'qore' createrole;
grant create, connect, temp on database ${QORE_DB_NAME} to ${QORE_DB_USER};
grant select on all tables in schema pg_catalog to ${QORE_DB_USER};
grant all on schema public to ${QORE_DB_USER};
EOF
    echo created pgsql user ${QORE_DB_USER} and db ${QORE_DB_NAME}

    # make sure we can access the DB
    qore -nX "(new Datasource(\"${QORE_DB_CONNSTR_PGSQL}\")).getServerVersion()"

    export POSTGRES_HOST=1
}

cleanup_postgres_on_host() {
    if [ "${POSTGRES_HOST}" = "1" ]; then
        # drop postgresql test user
        cat <<EOF | psql -Upostgres ${PSQL_ARGS}
drop owned by ${QORE_DB_USER};
drop database ${QORE_DB_NAME} with (force);
drop role ${QORE_DB_USER};
EOF
        echo dropped pgsql user ${QORE_DB_USER}
    fi
}

