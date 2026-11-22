# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the restore of a datasource by the name of its connection (qore#7016); run by SerializationCredentials.qtest in a
# process of its own, with QORE_CONNECTION_PROVIDERS and SQLUTIL_7016_DB set, as the connection registry reads the
# environment of the process

%modern
%requires SqlUtil
%requires Sqlite3SqlUtil
%requires ConnectionProvider

Sqlite3Table sub table(AbstractDatasource ds) {
    Sqlite3Table t(ds, "t7016");
    t.setupTable({"columns": {"id": {"qore_type": "integer", "notnull": True}}});
    return t;
}

# the datasource of a named connection: DatasourceConnection::getImpl() names it
{
    # live-provider-optin: the test's own sqlite connection with fake credentials
    Datasource ds = ConnectionProvider::get_connection("conn7016").get();
    on_exit ds.close();
    printf("name: %s\n", ds.getConnectionName());
    hash<auto> data = table(ds).serializeToData();
    string s = sprintf("%N", data);
    printf("password in data: %y\n", s.find("tiger-pass-7016") >= 0);
    printf("user in data: %y\n", s.find("scott-user-7016") >= 0);
    printf("masked config in data: %y\n", s.find("ds_masked") >= 0);
    printf("name in data: %y\n", s.find("conn7016") >= 0);
    AbstractTable restored = Serializable::deserialize(data);
    printf("restored: %s %s %d\n", restored.getName(), restored.getDatasource().getConnectionName(),
        restored.getDatasource().selectRow("select 1 as value").value);
    cast<Datasource>(restored.getDatasource()).close();
}

# a pool of a named connection is restored as a pool of the connection
{
    # live-provider-optin: the test's own sqlite connection with fake credentials
    Datasource ds = ConnectionProvider::get_connection("conn7016").get();
    DatasourcePool pool(ds.getConfigHash() + {"options": {"min": 1, "max": 2}});
    ds.close();
    pool.setConnectionName("conn7016");
    AbstractTable restored = Serializable::deserialize(table(pool).serializeToData());
    printf("restored pool: %s %s\n", restored.getDatasource().className(),
        restored.getDatasource().getConnectionName());
}

# a connection that is not a datasource connection is refused
{
    Datasource ds({"type": "sqlite3", "db": getenv("SQLUTIL_7016_DB")});
    on_exit ds.close();
    ds.setConnectionName("http7016");
    binary data = table(ds).serialize();
    try {
        auto restored = Serializable::deserialize(data);
        printf("not a datasource connection: restored %y\n", restored.className());
    } catch (hash<ExceptionInfo> ex) {
        printf("not a datasource connection: %s: %y\n", ex.err, ex.desc =~ /not a datasource connection/);
    }
}
