# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SchemaForeignConstraintAlignment class definition

/** SchemaForeignConstraintAlignment.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SqlUtil module
public namespace SqlUtil {
#! the foreign constraints of the tables of a schema alignment and the keys that they reference
/** @ref SqlUtil::AbstractDatabase::getAlignSql() "AbstractDatabase::getAlignSql()" creates an object of this class
    for the tables of a schema description and passes it to the alignment of each table, so that the foreign
    constraints between the tables of the schema are aligned whatever the order of the tables:
    - a key or index that a foreign constraint references is not dropped before the foreign constraint: the alignment
      of a table drops the foreign constraints that reference a key or index of the table in the database before it
      drops or recreates the key or index, on a database that refuses to drop it otherwise; the foreign constraints of
      the tables of the schema are read from the database before any table is aligned
    - a foreign constraint is added after the table that it references is aligned, so that the key that it
      references exists
    - a template with a foreign constraint that references columns of another table of the schema that are a unique
      key in the database, but not in the template, is refused before any SQL, on every database

    @since SqlUtil 3.11.0
*/
public class SchemaForeignConstraintAlignment {
    private {
        #! the template tables of the schema
        Tables templates;

        #! the tables of the schema in the database, by name, with their foreign constraints
        hash<string, AbstractTable> live;

        #! the tables aligned so far
        hash<string, bool> aligned;

        #! the foreign constraints that are added after every table is aligned
        /** each element is a hash with \c table (the name of the table of the constraint), \c sql_name (its name in
            SQL) and \c fk (the foreign constraint)
        */
        list<hash<auto>> deferred = ();
    }

    #! creates the object from the template tables of a schema and its tables in the database
    /** @param templates the template tables of the schema
        @param live the tables of the schema that exist in the database, by name; their foreign constraints are read
        from the database here
    */
    constructor(Tables templates, hash<string, AbstractTable> live) {
        self.templates = templates;
        self.live = live;
        map $1.getForeignConstraints(), live.iterator();
    }

    #! returns @ref True "True" if the foreign constraint references the given columns of the given table
    /** A foreign constraint references a key or index of its target table if its target columns are the columns of
        the key or index in any order, or the first columns of the index in order, as a MySQL foreign constraint can
        reference the leading columns of an index

        @param fk the foreign constraint
        @param table the name of the table
        @param columns the columns of a key or index of the table

        @return @ref True "True" if the foreign constraint references the columns of the table
    */
    static bool references(AbstractForeignConstraint fk, string table, list<string> columns) {
        if (fk.target.table != table && fk.target.table.lwr() != table.lwr()) {
            return False;
        }
        list<string> target_columns = fk.target.columns.keys();
        if (target_columns.size() > columns.size()) {
            return False;
        }
        if (columns[0..target_columns.size() - 1] == target_columns) {
            return True;
        }
        return target_columns.size() == columns.size() && fk.target.columns.matchKeys(columns);
    }

    #! raises an exception for a template with a foreign constraint that would not reference a unique key
    /** A foreign constraint must reference a unique key of its target table: a template whose foreign constraint
        references columns of another table of the schema that are a unique key of the table in the database, but
        not in the template of the table, changes the key without the foreign constraint, and is refused

        @throw FOREIGN-CONSTRAINT-ERROR a template's foreign constraint references columns of a table of the schema
        whose unique key in the database the template of the table changes or drops
    */
    checkTemplates() {
        foreach AbstractTable template in (templates.iterator()) {
            foreach AbstractForeignConstraint fk in (template.getForeignConstraints().iterator()) {
                string target = fk.target.table;
                if (!templates.hasKey(target) || !live{target}) {
                    continue;
                }
                list<string> columns = fk.target.columns.keys();
                if (SchemaForeignConstraintAlignment::findUniqueKey(templates{target}, columns)) {
                    continue;
                }
                *hash<auto> live_key = SchemaForeignConstraintAlignment::findUniqueKey(live{target}, columns);
                if (!live_key) {
                    continue;
                }
                *list<string> template_columns = SchemaForeignConstraintAlignment::getKeyColumns(templates{target},
                    live_key.name);
                string change = template_columns
                    ? sprintf("changes its unique key %y on them to (%s)", live_key.name, join(", ", template_columns))
                    : sprintf("drops its unique key %y on them", live_key.name);
                throw "FOREIGN-CONSTRAINT-ERROR", sprintf("foreign constraint %y of table %y references the columns "
                    "(%s) of table %y, whose template %s: the foreign constraint would not reference a unique key; "
                    "change the foreign constraint with the key", fk.getName(), template.getName(),
                    join(", ", columns), target, change);
            }
        }
    }

    #! returns the SQL that drops the foreign constraints of other tables that reference a key or index of a table
    /** Called by the alignment of a table before it drops or recreates a key or index: each foreign constraint of
        another table of the schema in the database that references the columns of the key or index is dropped
        first.  A table that is not aligned yet has the foreign constraint removed from its description in the
        database, so that its alignment adds the foreign constraint of its template.  A table that is aligned already
        kept a foreign constraint that its template has: it is dropped by the name that its template gives it, and
        added again after every table is aligned; one that its template does not have, or has with another
        definition, was dropped by its alignment

        @param table the name of the table whose key or index is dropped or recreated
        @param columns the columns of the key or index
        @param opt the alignment options, with any SQL callbacks

        @return the SQL that drops the foreign constraints
    */
    list<auto> getDropReferencingSql(string table, list<string> columns, *hash<auto> opt) {
        list<auto> l = ();
        foreach hash<auto> i in (live.pairIterator()) {
            if (i.key == table) {
                continue;
            }
            AbstractTable child = i.value;
            ForeignConstraints fks = child.getForeignConstraints();
            list<string> names = map $1.getName(), fks.iterator(),
                SchemaForeignConstraintAlignment::references($1, table, columns);
            foreach string fk_name in (names) {
                AbstractForeignConstraint fk = fks{fk_name};
                AbstractForeignConstraint drop_fk = fk;
                if (aligned{i.key}) {
                    *AbstractForeignConstraint template_fk = getTemplateForeignConstraint(i.key, fk);
                    if (!template_fk) {
                        continue;
                    }
                    drop_fk = template_fk;
                    deferred += {"table": i.key, "sql_name": child.getSqlName(), "fk": template_fk};
                }
                l += AbstractDatabase::doCallback(opt, drop_fk.getDropSql(child.getSqlName()),
                    AbstractDatabase::AC_Drop, "foreign constraint", drop_fk.getName(), i.key);
                fks.take(fk_name);
            }
        }
        return l;
    }

    #! defers adding a foreign constraint until the table that it references is aligned
    /** @param table the name of the table of the foreign constraint
        @param sql_name the name of the table in SQL
        @param fk the foreign constraint

        @return @ref True "True" if the foreign constraint references another table of the schema that is not aligned
        yet, so that it is added after every table is aligned, @ref False "False" if it can be added now
    */
    bool deferAdd(string table, string sql_name, AbstractForeignConstraint fk) {
        string target = fk.target.table;
        if (target == table || !templates.hasKey(target) || aligned{target}) {
            return False;
        }
        deferred += {"table": table, "sql_name": sql_name, "fk": fk};
        return True;
    }

    #! records that a table is aligned
    /** @param table the name of the table
    */
    tableAligned(string table) {
        aligned{table} = True;
    }

    #! returns the SQL that adds the deferred foreign constraints, after every table is aligned
    /** @param opt the alignment options, with any SQL callbacks

        @return the SQL that adds the foreign constraints
    */
    list<auto> getDeferredSql(*hash<auto> opt) {
        list<hash<auto>> todo = deferred;
        deferred = ();
        list<auto> l = ();
        foreach hash<auto> d in (todo) {
            AbstractForeignConstraint fk = d.fk;
            l += AbstractDatabase::doCallback(opt, fk.getCreateSql(d.sql_name, opt), AbstractDatabase::AC_Add,
                "foreign constraint", fk.getName(), d.table);
        }
        return l;
    }

    #! returns the foreign constraint of the template of a table that an aligned table kept for one in the database
    /** @return the template's foreign constraint with the name of the given one and the same definition, or one with
        the same definition under another name, which the alignment renamed it to; @ref nothing if the template has
        none, so that the alignment dropped it
    */
    private *AbstractForeignConstraint getTemplateForeignConstraint(string table, AbstractForeignConstraint fk) {
        if (!templates.hasKey(table)) {
            return;
        }
        ForeignConstraints template_fks = templates{table}.getForeignConstraints();
        if (template_fks.hasKey(fk.getName())) {
            AbstractForeignConstraint template_fk = template_fks{fk.getName()};
            return template_fk.equal(fk) ? template_fk : NOTHING;
        }
        return template_fks.findEqual(fk);
    }

    #! returns the name and columns of a unique key of a table with the given columns in any order
    /** A unique key is the primary key, a unique constraint, or a unique index

        @return a hash with \c name and \c columns, or @ref nothing if the table has no unique key on the columns
    */
    private static *hash<auto> findUniqueKey(AbstractTable table, list<string> columns) {
        AbstractPrimaryKey pk = table.getPrimaryKey();
        if (!pk.empty() && pk.matchKeys(columns)) {
            return {"name": pk.getName(), "columns": pk.keys()};
        }
        foreach AbstractConstraint c in (table.getConstraints().iterator()) {
            if (c instanceof AbstractUniqueConstraint && cast<AbstractUniqueConstraint>(c).matchKeys(columns)) {
                return {"name": c.getName(), "columns": cast<AbstractUniqueConstraint>(c).keys()};
            }
        }
        foreach AbstractIndex ix in (table.getIndexes().iterator()) {
            if (ix.unique && ix.columns.matchKeys(columns)) {
                return {"name": ix.name, "columns": ix.columns.keys()};
            }
        }
    }

    #! returns the columns of the key or index of a table with the given name
    /** @return the columns of the primary key, unique constraint or index with the name, or @ref nothing if the
        table has none
    */
    private static *list<string> getKeyColumns(AbstractTable table, string name) {
        AbstractPrimaryKey pk = table.getPrimaryKey();
        if (!pk.empty() && pk.getName() == name) {
            return pk.keys();
        }
        Constraints constraints = table.getConstraints();
        if (constraints.hasKey(name) && constraints{name} instanceof AbstractUniqueConstraint) {
            return cast<AbstractUniqueConstraint>(constraints{name}).keys();
        }
        Indexes indexes = table.getIndexes();
        if (indexes.hasKey(name)) {
            return indexes{name}.columns.keys();
        }
    }
}
}
