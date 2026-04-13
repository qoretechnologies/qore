# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SqlUtil auto-trigger generation helper

/** AutoTriggerHelper.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

%modern

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {

/** @defgroup auto_trigger_config Auto-Trigger Configuration

    Auto-trigger configuration allows automatic generation of standard database triggers for common
    patterns like timestamp management and sequence-based ID population.

    The configuration is a hash with the following keys:
    - \c created: (bool) if \c True, auto-populate the \c created column with the current timestamp on INSERT
    - \c modified: (bool) if \c True, auto-populate the \c modified column with the current timestamp on
      INSERT and UPDATE
    - \c sequence_columns: (hash<string, string>) a hash mapping column names to sequence names; when a row
      is inserted with a NULL value for the column, the next value from the sequence is used

    @par Example Configuration
    @code{.py}
    hash<auto> auto_triggers = {
        "created": True,
        "modified": True,
        "sequence_columns": {
            "id": "seq_my_table",
        },
    };
    @endcode

    @since SqlUtil 1.10
*/

#! Generates auto-trigger definitions for a table based on the given configuration
/** Returns a hash that can be merged into a table description hash. The returned hash contains
    driver-specific trigger definitions under the \c "triggers" and \c "driver" keys.

    @param driver_name the database driver name (e.g., \c "pgsql", \c "oracle", \c "mysql")
    @param table_name the name of the table
    @param auto_trigger_config the auto-trigger configuration hash; see @ref auto_trigger_config
    @param insert_only if \c True, only INSERT triggers are generated (no UPDATE triggers for \c modified)

    @return a hash with the following possible keys:
    - \c "triggers": hash of trigger name to trigger source SQL (driver-specific)
    - \c "driver": hash with driver-specific extra definitions (e.g., PostgreSQL trigger functions)

    @throw AUTO-TRIGGER-ERROR unsupported driver or invalid configuration

    @since SqlUtil 1.10
*/
public hash<auto> sub generate_auto_triggers(string driver_name, string table_name,
        hash<auto> auto_trigger_config, bool insert_only = False, *list<string> pk_columns) {
    bool has_created = auto_trigger_config.created.toBool();
    bool has_modified = auto_trigger_config.modified.toBool();
    *hash<string, string> seq_cols = auto_trigger_config.sequence_columns;

    # the caller can force insert_only via the positional argument, or the schema can request it
    # via the auto_triggers.insert_only key (useful for insert-only tables that still carry a
    # modified column the application maintains by hand)
    bool effective_insert_only = insert_only || auto_trigger_config.insert_only.toBool();

    if (!has_created && !has_modified && !seq_cols) {
        return {};
    }

    switch (driver_name) {
        case "pgsql":
            return generate_pgsql_auto_triggers(table_name, has_created, has_modified, seq_cols,
                effective_insert_only);
        case "oracle":
            return generate_oracle_auto_triggers(table_name, has_created, has_modified, seq_cols,
                effective_insert_only);
        case "mysql":
            return generate_mysql_auto_triggers(table_name, has_created, has_modified, seq_cols,
                effective_insert_only);
        case "mssql":
            return generate_mssql_auto_triggers(table_name, has_created, has_modified, seq_cols,
                effective_insert_only, pk_columns);
        default:
            throw "AUTO-TRIGGER-ERROR", sprintf("auto-trigger generation is not supported for driver %y; "
                "supported drivers: pgsql, oracle, mysql, mssql", driver_name);
    }
}

#! Generates auto-trigger definitions for all supported drivers
/** Returns a hash with trigger definitions for all supported drivers, suitable for driver-independent
    table descriptions.

    @param table_name the name of the table
    @param auto_trigger_config the auto-trigger configuration hash; see @ref auto_trigger_config
    @param insert_only if \c True, only INSERT triggers are generated

    @return a hash with \c "triggers" and \c "driver" keys containing per-driver trigger definitions

    @since SqlUtil 1.10
*/
public hash<auto> sub generate_auto_triggers_all_drivers(string table_name,
        hash<auto> auto_trigger_config, bool insert_only = False) {
    bool has_created = auto_trigger_config.created.toBool();
    bool has_modified = auto_trigger_config.modified.toBool();
    *hash<string, string> seq_cols = auto_trigger_config.sequence_columns;

    if (!has_created && !has_modified && !seq_cols) {
        return {};
    }

    hash<auto> pgsql = generate_pgsql_auto_triggers(table_name, has_created, has_modified, seq_cols,
        insert_only);
    hash<auto> oracle = generate_oracle_auto_triggers(table_name, has_created, has_modified, seq_cols,
        insert_only);
    hash<auto> mysql = generate_mysql_auto_triggers(table_name, has_created, has_modified, seq_cols,
        insert_only);

    return {
        "triggers": {
            "driver": {
                "oracle": oracle.triggers,
                "pgsql": pgsql.triggers,
                "mysql": mysql.triggers,
            },
        },
        "driver": {
            "pgsql": pgsql.driver.pgsql ?? {},
        },
    };
}

# --- PostgreSQL ---

hash<auto> sub generate_pgsql_auto_triggers(string name, bool has_created, bool has_modified,
        *hash<string, string> seq_cols, bool insert_only) {
    string func_name = "trig_" + name + "()";
    string trig_name = "trig_" + name;

    # Build trigger function body
    string sql = "returns trigger language plpgsql as $function$\n"
        "begin\n"
        "  if (tg_op = 'INSERT') then\n";

    # Sequence columns: auto-populate from sequence if NULL
    if (seq_cols) {
        foreach hash<auto> sc in (seq_cols.pairIterator()) {
            sql += sprintf("    if new.%s is null then\n"
                "       select nextval('%s') into new.%s;\n"
                "    end if;\n", sc.key, sc.value, sc.key);
        }
    }

    # Created timestamp
    if (has_created) {
        sql += "    if new.created is null then\n"
            "      select current_timestamp into new.created;\n"
            "    end if;\n";
    }

    # Modified timestamp (on INSERT)
    if (has_modified) {
        sql += "    if new.modified is null then\n"
            "      select current_timestamp into new.modified;\n"
            "    end if;\n";
    }

    if (insert_only) {
        sql += "  end if;\n";
    } else {
        sql += "  else\n"
            "    if (tg_op = 'UPDATE') then\n";

        if (has_modified) {
            sql += "      if new.modified is null or new.modified = old.modified then\n"
                "        select current_timestamp into new.modified;\n"
                "      end if;\n";
        }

        sql += "    end if;\n"
            "  end if;\n";
    }

    sql += "  return new;\n"
        "end;\n"
        "$function$";

    # Build trigger definition
    string trig_sql = sprintf("before insert %son %s for each row execute procedure %s",
        insert_only ? "" : "or update ", name, func_name);

    return {
        "triggers": {
            trig_name: trig_sql,
        },
        "driver": {
            "pgsql": {
                "functions": {
                    func_name: sql,
                },
            },
        },
    };
}

# --- Oracle ---

hash<auto> sub generate_oracle_auto_triggers(string name, bool has_created, bool has_modified,
        *hash<string, string> seq_cols, bool insert_only) {
    string trig_name = "trig_" + name;

    string sql = sprintf("BEFORE INSERT%s ON %s\n"
        "REFERENCING NEW AS NEW OLD AS OLD\n"
        "FOR EACH ROW\n"
        "begin\n"
        "  if inserting then\n", insert_only ? "" : " OR UPDATE", name.upr());

    # Sequence columns
    if (seq_cols) {
        foreach hash<auto> sc in (seq_cols.pairIterator()) {
            sql += sprintf("    if :new.%s is null then\n"
                "      select %s.nextval into :new.%s from dual;\n"
                "    end if;\n", sc.key, sc.value, sc.key);
        }
    }

    # Created timestamp
    if (has_created) {
        sql += "    if :new.created is null then\n"
            "      :new.created := localtimestamp;\n"
            "    end if;\n";
    }

    # Modified timestamp on INSERT only (when insert_only)
    if (has_modified && insert_only) {
        sql += "    if :new.modified is null then\n"
            "      :new.modified := localtimestamp;\n"
            "    end if;\n";
    }

    sql += "  end if;\n";

    # Modified timestamp (on INSERT or UPDATE)
    if (has_modified && !insert_only) {
        sql += "  --\n"
            "  if :new.modified is null or :new.modified = :old.modified then\n"
            "    :new.modified := localtimestamp;\n"
            "  end if;\n";
    }

    sql += "end;";

    return {
        "triggers": {
            trig_name: sql,
        },
    };
}

# --- MySQL ---

hash<auto> sub generate_mysql_auto_triggers(string name, bool has_created, bool has_modified,
        *hash<string, string> seq_cols, bool insert_only) {
    string insert_trig = "trig_" + name + "_insert";

    # INSERT trigger
    string sql = "before insert on " + name + " for each row\nbegin\n";

    # Sequence columns
    if (seq_cols) {
        foreach hash<auto> sc in (seq_cols.pairIterator()) {
            sql += sprintf("  if new.%s is null or new.%s = 0 then\n"
                "     set new.%s = my_nextval('%s');\n"
                "  end if;\n", sc.key, sc.key, sc.key, sc.value);
        }
    }

    # Created timestamp
    if (has_created) {
        sql += "  if new.created is null then\n"
            "    set new.created = now();\n"
            "  end if;\n";
    }

    # Modified timestamp (on INSERT)
    if (has_modified) {
        sql += "  if new.modified is null then\n"
            "    set new.modified = now();\n"
            "  end if;\n";
    }

    sql += "end";

    hash<auto> triggers = {
        insert_trig: sql,
    };

    # UPDATE trigger for modified timestamp
    if (has_modified && !insert_only) {
        string update_trig = "trig_" + name + "_update";
        triggers{update_trig} = "before update on " + name + " for each row\n"
            "begin\n"
            "  if new.modified is null or new.modified = old.modified then\n"
            "    set new.modified = now();\n"
            "  end if;\n"
            "end";
    }

    return {
        "triggers": triggers,
    };
}

# --- MSSQL ---

hash<auto> sub generate_mssql_auto_triggers(string name, bool has_created, bool has_modified,
        *hash<string, string> seq_cols, bool insert_only, *list<string> pk_columns) {
    # MSSQL uses DEFAULT constraints for created/modified and IDENTITY for sequences typically,
    # but we generate triggers for consistency with the auto-trigger pattern
    string trig_name = "trig_" + name;

    # Build PK join condition; defaults to "id" if not specified
    if (!pk_columns) {
        pk_columns = ("id",);
    }
    string pk_join = (map sprintf("t.%s = i.%s", $1, $1), pk_columns).join(" AND ");
    string pk_join_d = (map sprintf("d.%s = i.%s", $1, $1), pk_columns).join(" AND ");

    string sql = sprintf("ON %s\nAFTER INSERT%s\nAS\nBEGIN\n  SET NOCOUNT ON;\n",
        name, insert_only ? "" : ", UPDATE");

    # Created timestamp (only on INSERT)
    if (has_created) {
        sql += sprintf("  UPDATE t SET t.created = GETDATE()\n"
            "  FROM %s t INNER JOIN inserted i ON %s\n"
            "  WHERE i.created IS NULL;\n", name, pk_join);
    }

    # Modified timestamp
    if (has_modified && !insert_only) {
        sql += sprintf("  IF UPDATE(modified) OR NOT EXISTS (SELECT 1 FROM deleted)\n"
            "  BEGIN\n"
            "    UPDATE t SET t.modified = GETDATE()\n"
            "    FROM %s t INNER JOIN inserted i ON %s\n"
            "    WHERE i.modified IS NULL OR i.modified = (SELECT d.modified FROM deleted d WHERE %s);\n"
            "  END\n", name, pk_join, pk_join_d);
    }

    sql += "END";

    return {
        "triggers": {
            trig_name: sql,
        },
    };
}

} # namespace SqlUtil
