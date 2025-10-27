# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DbDataProviderBase class definition

/** DbDataProviderBase.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the DbDataProvider module
public namespace DbDataProvider {
#! The database data provider base class
public class DbDataProviderBase inherits AbstractDataProvider {
    private {
        #! the database object, if required
        AbstractDatabase db;

        #! Constructor options
        const ConstructorOptions = {
            "datasource": <DataProviderOptionInfo>{
                "display_name": "Datasource",
                "short_desc": "The datasource connection string to use (or an 'AbstractDatasource' object)",
                "type": (
                    AbstractDataProviderType::get(StringType),
                    AbstractDataProviderType::get(new Type("AbstractDatasource")),
                ),
                "desc": "The datasource connection string or `AbstractDatasource` object",
                "required": True,
            },
        };

        #! Mapper runtime key info; see details below
        /** The DbDataProvider class supports the following create option:
            - \c sequence: names the DB sequence that will be used to populate the field
            - \c sequence_currval: names the DB sequence that will be used to populate the field; the current value of
              the sequence is used; will not increment the sequence
        */
        const MapperKeyInfo = Mapper::MapperKeyInfo + {
            "sequence": <MapperRuntimeKeyInfo>{
                "desc": "names the DB sequence that will be used to populate the field",
                "value_type": "string",
                "unique_roles": "*",
                "returns_type": "int",
            },
            "sequence_currval": <MapperRuntimeKeyInfo>{
                "desc": "names the DB sequence that will be used to populate the field; the current value of the "
                    "sequence is used; will not increment the sequence",
                "value_type": "string",
                "unique_roles": "*",
                "returns_type": "int",
            },
        };

        #! Lookup to get an abstract datasource from a name
        static *code datasource_lookup;

        #! Lookup to get an abstract table from a datasource and a name
        static *code table_lookup;
    }

    #! creates the object
    constructor(AbstractDatasource ds, *hash<auto> opts, *LoggerInterface logger) : AbstractDataProvider(logger) {
        db = AbstractDatabase::getDatabase(ds, opts);
    }

    #! creates the object
    constructor(AbstractDatabase db, *LoggerInterface logger) : AbstractDataProvider(logger) {
        self.db = db;
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options, *LoggerInterface logger) : AbstractDataProvider(logger) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        AbstractDatasource ds;
        if (copts.datasource instanceof AbstractDatasource) {
            ds = copts.datasource;
        } else {
            ds = getDatasource(copts.datasource);
        }
        db = AbstractDatabase::getDatabase(ds);
    }

    #! Creates the object with the given logger
    constructor(*LoggerInterface logger) : AbstractDataProvider(logger) {
    }

    #! Returns @ref True if the data provider supports transaction management
    /** @return @ref True if the data provider supports transaction management, in which case commit() or rollback()
        must be called to flush or discard data written to the data provider
    */
    bool requiresTransactionManagement() {
        return True;
    }

    #! Begins a transaction in the datasource
    /**
        @see:
        - commit()
        - rollback()

        @since DbDataProvider 1.0.1
    */
    beginTransaction() {
        db.beginTransaction();
    }

    #! Commits any transaction in progress in the datasource
    /**
        @see:
        - beginTransaction()
        - rollback()
    */
    commit() {
        db.commit();
    }

    #! Rolls back any transaction in progress in the datasource
    /**
        @see:
        - beginTransaction()
        - commit()
    */
    rollback() {
        db.rollback();
    }

    #! Returns @ref True if the current thread is in a transaction
    /** @return @ref True if the current thread is in a transaction
    */
    private bool currentThreadInTransactionImpl() {
        return db.getDatasource().currentThreadInTransaction();
    }

    #! Returns custom data mapper runtime keys
    /** @return custom data mapper runtime keys
    */
    *hash<string, hash<MapperRuntimeKeyInfo>> getMapperRuntimeKeys() {
        hash<string, hash<MapperRuntimeKeyInfo>> rv = MapperKeyInfo;
        rv.sequence.handler = \doSequenceKey();
        rv.sequence_currval.handler = \doSequenceCurrvalKey();
        return rv;
    }

    #! Processes the \c sequence runtime key in mappers as a mapper key handler
    int doSequenceKey(string sequence_name, hash<auto> ctx, *reference<bool> missing_input) {
        on_error rethrow $1.err, sprintf("%s (mapper 'sequence' error; sequence: %y)", $1.desc, sequence_name);
        return db.getNextSequenceValue(sequence_name);
    }

    #! Processes the \c sequence_currval runtime key in mappers as a mapper key handler
    int doSequenceCurrvalKey(string sequence_name, hash<auto> ctx, *reference<bool> missing_input) {
        on_error rethrow $1.err, sprintf("%s (mapper 'sequence_currval' error; sequence: %y)", $1.desc, sequence_name);
        return db.getCurrentSequenceValue(sequence_name);
    }

    #! Returns a table description hash from a field description
    /** @throw TABLE-DESC-ERROR error in table description
    */
    hash<auto> makeTableDesc(string name, hash<string, AbstractDataField> fields, *hash<auto> opts) {
        hash<string, hash<GenericColumnInfo>> cols = map {$1.key: getColumnDesc(db, $1.value, opts)},
            fields.pairIterator();

        hash<auto> tdesc += {
            "columns": cols,
        };
        # process PK
        if (opts.primary_key) {
            foreach auto cname in (opts.primary_key) {
                if (cname.typeCode() != NT_STRING) {
                    throw "TABLE-DESC-ERROR", sprintf("table %y primary key column value %d/%d (starting from 1) is "
                        "type %y; expecting \"string\"", name, $# + 1, opts.primary_key.size(), name.type());
                }
                if (!fields{cname}) {
                    throw "TABLE-DESC-ERROR", sprintf("table %y primary key column %y is not present in the input "
                        "field description; known fields: %y", name, cname, keys fields);
                }
            }
            string pk_name = opts.primary_key_name ?? sprintf("pk_%s", name);
            tdesc.primary_key = {
                "name": pk_name,
                "columns": opts.primary_key,
            };
        }
        # process indexes
        if (opts.indexes) {
            tdesc.indexes = opts.indexes;
        }
        # process foreign_constraints
        if (opts.foreign_constraints) {
            tdesc.foreign_constraints = opts.foreign_constraints;
        }

        return tdesc;
    }

    #! Returns a column description hash for a field
    static hash<GenericColumnInfo> getColumnDesc(AbstractDatabase db, AbstractDataField field, *hash<auto> opts) {
        AbstractDataProviderType type = field.getType();
        hash<GenericColumnInfo> col({
            "default_value": field.getDefaultValue(),
            "notnull": !field.isAssignableFrom(Reflection::NothingType),
        });

        string type_name = type.getName();
        switch (type.getBaseTypeCode()) {
            case NT_INT:
                type_name = "integer";
                break;

            case NT_STRING:
                if (*int size = field.getType().getOptionValue("string.max_size_chars")) {
                    col.size = size;
                }
                break;

            case NT_NUMBER:
                if (db.requiresScale()) {
                    col.size = db.getMaximumPrecision();
                    col.scale = col.size / 3;
                }
                break;
        }
        col.qore_type = type_name;

        # add field description as a comment if possible
        if (*string desc = field.getDescription()) {
            col.comment = desc;
        }
        return col;
    }

    #! Sets the datasource lookup
    static setDatasourceLookup(code datasource_lookup) {
        DbDataProvider::datasource_lookup = datasource_lookup;
    }

    #! Sets the table lookup
    static setTableLookup(code table_lookup) {
        DbDataProvider::table_lookup = table_lookup;
    }

    #! Returns the given AbstractDatasource object
    static AbstractDatasource getDatasource(AbstractDatasource ds) {
        return ds;
    }

    #! Returns an AbstractDatasource object from the given string
    static AbstractDatasource getDatasource(string ds_string) {
        return datasource_lookup && (ds_string !~ /:/)
            ? DbDataProvider::datasource_lookup(ds_string)
            : new Datasource(ds_string);
    }

    #! Returns an AbstractTable object from the given datasource and table strings
    static AbstractTable getTable(string ds_string, string table_string) {
        if (table_lookup && (ds_string !~ /:/)) {
            return DbDataProvider::table_lookup(ds_string, table_string);
        }
        AbstractDatasource ds = DbDataProvider::getDatasource(ds_string);
        return AbstractTable::getTable(ds, table_string);
    }

    #! Returns an AbstractTable object from the given datasource and table arguments
    static AbstractTable getTable(AbstractDatasource ds, string table_string) {
        if (table_lookup) {
            return DbDataProvider::table_lookup(ds, table_string);
        }
        return AbstractTable::getTable(ds, table_string);
    }
}
}
