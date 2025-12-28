#!/bin/sh

REDIS_CONTAINER="qore-test-redis"
REDIS_PORT=6379

start_redis() {
    docker run --name=${REDIS_CONTAINER} --network=host -d redis:latest

    # wait for Redis server to start
    printf "waiting on Redis server: "
    waited=0
    while true; do
        pong=`docker exec ${REDIS_CONTAINER} redis-cli ping 2>/dev/null`
        if [ "$pong" = "PONG" ]; then
            echo ": Redis server started"
            break
        fi

        # 30 second time limit
        if [ $waited -eq 30 ]; then
            echo && echo "Waited too long for Redis to start; aborting build."
            exit 1
        fi
        printf .
        # sleep for 1 second
        sleep 1
        waited=$((waited+1))
    done

    export REDIS_HOST=localhost
    export REDIS_PORT=${REDIS_PORT}
    export REDIS_URL="redis://${REDIS_HOST}:${REDIS_PORT}"

    # make sure we can access Redis
    docker exec ${REDIS_CONTAINER} redis-cli ping
}

setup_redis_on_host() {
    # add env vars to environment file and load it
    echo export REDIS_HOST=${RUNNER_HOST:=redis} >> /tmp/env.sh
    echo export REDIS_PORT=${REDIS_PORT} >> /tmp/env.sh
    echo export REDIS_URL="redis://${RUNNER_HOST:=redis}:${REDIS_PORT}" >> /tmp/env.sh

    . /tmp/env.sh

    export REDIS_ON_HOST=1
}

stop_redis() {
    docker stop ${REDIS_CONTAINER} 2>/dev/null
    docker rm ${REDIS_CONTAINER} 2>/dev/null
}

cleanup_redis_on_host() {
    if [ "${REDIS_ON_HOST}" = "1" ]; then
        echo "Redis cleanup: flushing test database"
        # Optionally flush the test database
        # docker exec ${REDIS_CONTAINER} redis-cli FLUSHDB 2>/dev/null || true
    fi
}
