/* Copyright 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <nats/nats.h>
#include <stdio.h>
#include <string.h>

#define CHECK(call) do { \
    natsStatus status = (call); \
    if (status != NATS_OK) { \
        fprintf(stderr, "%s: %s\n", #call, natsStatus_GetText(status)); \
        goto cleanup; \
    } \
} while (0)
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Assertion failed: %s\n", #condition); \
        goto cleanup; \
    } \
} while (0)

static void reply(natsConnection *connection, natsSubscription *subscription,
                  natsMsg *message, void *closure) {
    (void) subscription;
    (void) closure;
    /* A failed reply makes the bounded request below fail with a timeout. */
    natsConnection_PublishString(connection, natsMsg_GetReply(message), "pong");
    natsMsg_Destroy(message);
}

int main(int argc, char **argv) {
    natsConnection *connection = NULL;
    natsSubscription *subscription = NULL;
    natsSubscription *responder = NULL;
    natsMsg *message = NULL;
    jsCtx *jetstream = NULL;
    kvStore *store = NULL;
    kvEntry *entry = NULL;
    kvConfig config;
    uint64_t first_revision = 0, second_revision = 0;
    int result = 1;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NATS_URL\n", argv[0]);
        return 2;
    }
    CHECK(natsConnection_ConnectTo(&connection, argv[1]));
    CHECK(natsConnection_SubscribeSync(&subscription, connection, "qore.pubsub"));
    CHECK(natsConnection_FlushTimeout(connection, 2000));
    CHECK(natsConnection_PublishString(connection, "qore.pubsub", "payload"));
    CHECK(natsSubscription_NextMsg(&message, subscription, 2000));
    REQUIRE(natsMsg_GetDataLength(message) == 7);
    REQUIRE(memcmp(natsMsg_GetData(message), "payload", 7) == 0);
    natsMsg_Destroy(message);
    message = NULL;

    CHECK(natsConnection_Subscribe(&responder, connection, "qore.request", reply, NULL));
    CHECK(natsConnection_FlushTimeout(connection, 2000));
    CHECK(natsConnection_RequestString(&message, connection, "qore.request", "ping", 2000));
    REQUIRE(natsMsg_GetDataLength(message) == 4);
    REQUIRE(memcmp(natsMsg_GetData(message), "pong", 4) == 0);
    natsMsg_Destroy(message);
    message = NULL;

    CHECK(natsConnection_JetStream(&jetstream, connection, NULL));
    CHECK(kvConfig_Init(&config));
    config.Bucket = "qore_qualification";
    config.History = 2;
    config.StorageType = js_MemoryStorage;
    CHECK(js_CreateKeyValue(&store, jetstream, &config));
    REQUIRE(kvStore_Get(&entry, store, "missing") == NATS_NOT_FOUND);
    CHECK(kvStore_PutString(&first_revision, store, "item", "first"));
    CHECK(kvStore_PutString(&second_revision, store, "item", "second"));
    REQUIRE(first_revision > 0 && second_revision > first_revision);
    CHECK(kvStore_GetRevision(&entry, store, "item", first_revision));
    REQUIRE(kvEntry_ValueLen(entry) == 5);
    REQUIRE(memcmp(kvEntry_Value(entry), "first", 5) == 0);
    kvEntry_Destroy(entry);
    entry = NULL;
    CHECK(kvStore_Get(&entry, store, "item"));
    REQUIRE(kvEntry_Revision(entry) == second_revision);
    REQUIRE(kvEntry_ValueLen(entry) == 6);
    REQUIRE(memcmp(kvEntry_Value(entry), "second", 6) == 0);
    CHECK(js_DeleteKeyValue(jetstream, config.Bucket));
    result = 0;
    puts("PASS: installed C API pub/sub, request/reply and JetStream key/value history");

cleanup:
    kvEntry_Destroy(entry);
    kvStore_Destroy(store);
    jsCtx_Destroy(jetstream);
    natsMsg_Destroy(message);
    natsSubscription_Destroy(subscription);
    natsSubscription_Destroy(responder);
    natsConnection_Destroy(connection);
    if (nats_CloseAndWait(5000) != NATS_OK) {
        fputs("NATS shutdown timed out\n", stderr);
        result = 1;
    }
    return result;
}
