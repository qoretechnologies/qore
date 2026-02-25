# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeneratorRecordIterator class definition

/** GeneratorRecordIterator.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeneratorDataProvider module
public namespace GeneratorDataProvider {

#! Type map from generator field type strings to DataProvider types
const GeneratorTypeMap = {
    GeneratorFieldType::String: AbstractDataProviderTypeMap."string",
    GeneratorFieldType::Int: AbstractDataProviderTypeMap."int",
    GeneratorFieldType::Float: AbstractDataProviderTypeMap."float",
    GeneratorFieldType::Bool: AbstractDataProviderTypeMap."bool",
    GeneratorFieldType::Date: AbstractDataProviderTypeMap."date",
};

#! Default field definitions used when no custom schema is provided
public const DefaultFields = (
    {"name": "id", "type": GeneratorFieldType::Int, "desc": "Record identifier"},
    {"name": "name", "type": GeneratorFieldType::String, "desc": "Record name"},
    {"name": "value", "type": GeneratorFieldType::Float, "desc": "Numeric value"},
    {"name": "active", "type": GeneratorFieldType::Bool, "desc": "Active flag"},
    {"name": "created", "type": GeneratorFieldType::Date, "desc": "Creation date"},
);

#! Record iterator that generates configurable test records in-memory
public class GeneratorRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private:internal {
        #! Total records to generate
        int count;

        #! Current record index (0-based)
        int current = -1;

        #! Field definitions: list of hashes with "name", "type", and optionally "desc"
        list<auto> fields;

        #! Data generation strategy
        string strategy;

        #! Whether to include null values randomly
        bool include_nulls;

        #! Template record for template-based generation
        *hash<auto> template_record;

        #! Record type definition
        hash<string, DataProvider::AbstractDataField> record_type;

        #! Fixed base date captured at construction time for deterministic date generation
        date base_date;

        #! Local PRNG state for seeded generation; avoids mutating global RNG via srand()
        *int prng_state;
    }

    #! Creates the iterator
    /** @param count the number of records to generate
        @param fields list of field definition hashes with "name" and "type" keys
        @param strategy the generation strategy: GeneratorStrategy::Sequential or GeneratorStrategy::Random
        @param include_nulls if True and strategy is "random", ~10% of values will be NOTHING
        @param seed optional random seed for reproducible output
        @param template_record optional template record for template-based generation
        @param base_date optional fixed base date for date field generation; when provided, date values are
        computed relative to this date for full cross-run reproducibility; when not provided, the current
        date/time is used
    */
    constructor(int count, list<auto> fields, string strategy = GeneratorStrategy::Sequential,
            bool include_nulls = False, *int seed, *hash<auto> template_record,
            *date base_date) {
        self.count = count;
        self.fields = fields;
        self.strategy = strategy;
        self.include_nulls = include_nulls;
        self.template_record = template_record;
        self.base_date = base_date ?? now_us();

        if (exists seed) {
            # initialize local PRNG state from seed; uses a simple LCG to avoid
            # polluting the global RNG state via srand()
            prng_state = seed;
        }

        # Build record type from field definitions
        map record_type{$1.name} = makeFieldDef($1), fields;
    }

    #! Returns @ref True if the iterator is valid
    bool valid() {
        return current >= 0 && current < count;
    }

    #! Returns the current record
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "iterator is not pointing at a valid element";
        }
        return generateRecord(current);
    }

    #! Returns the record description, if available
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_type;
    }

    #! Increments the row pointer
    /** @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        return ++current < count;
    }

    #! Returns the next pseudo-random non-negative int
    /** When a seed was provided, uses a local linear congruential generator (LCG) that does not
        touch the global RNG state.  Otherwise falls back to the global rand().
    */
    private int nextRand() {
        if (exists prng_state) {
            # LCG parameters from Numerical Recipes
            prng_state = (prng_state * 1664525 + 1013904223) & 0x7fffffff;
            return prng_state;
        }
        return rand();
    }

    #! Generates a single record for the given index
    private hash<auto> generateRecord(int idx) {
        hash<auto> rec = {};
        foreach hash<auto> field in (fields) {
            if (include_nulls && strategy == GeneratorStrategy::Random && nextRand() % 10 == 0) {
                rec{field.name} = NOTHING;
                continue;
            }
            rec{field.name} = template_record
                ? varyValue(template_record{field.name}, idx, field)
                : generateValue(field, idx);
        }
        return rec;
    }

    #! Generates a value based on field type and strategy
    private auto generateValue(hash<auto> field, int idx) {
        switch (field.type) {
            case GeneratorFieldType::String:
                return strategy == GeneratorStrategy::Random
                    ? sprintf("str_%d_%d", idx, nextRand())
                    : sprintf("%s_%d", field.name, idx);
            case GeneratorFieldType::Int:
                return strategy == GeneratorStrategy::Random ? nextRand() % 100000 : idx;
            case GeneratorFieldType::Float:
                return strategy == GeneratorStrategy::Random
                    ? (nextRand() % 100000).toFloat() / 100.0
                    : idx.toFloat();
            case GeneratorFieldType::Bool:
                return strategy == GeneratorStrategy::Random ? (nextRand() % 2 == 0) : (idx % 2 == 0);
            case GeneratorFieldType::Date:
                return strategy == GeneratorStrategy::Random
                    ? base_date - days(nextRand() % 3650)
                    : base_date - days(idx);
        }
        # fallback for unknown types
        return sprintf("val_%d", idx);
    }

    #! Varies a template value for template-based generation
    private auto varyValue(auto val, int idx, hash<auto> field) {
        if (strategy == GeneratorStrategy::Random) {
            switch (field.type) {
                case GeneratorFieldType::String:
                    return val + "_" + string(nextRand() % 10000);
                case GeneratorFieldType::Int:
                    return val + nextRand() % 1000;
                case GeneratorFieldType::Float:
                    return val + (nextRand() % 10000).toFloat() / 100.0;
                case GeneratorFieldType::Bool:
                    return nextRand() % 2 == 0;
                case GeneratorFieldType::Date:
                    return val + days(nextRand() % 365);
            }
            return val;
        }
        switch (field.type) {
            case GeneratorFieldType::String:
                return val + "_" + string(idx);
            case GeneratorFieldType::Int:
                return val + idx;
            case GeneratorFieldType::Float:
                return val + idx.toFloat() * 0.1;
            case GeneratorFieldType::Bool:
                return idx % 2 == 0 ? val : !val;
            case GeneratorFieldType::Date:
                return val + days(idx);
        }
        return val;
    }

    #! Creates a DataField from a field definition hash
    private DataProvider::AbstractDataField makeFieldDef(hash<auto> field) {
        return new QoreDataField(field.name, field.desc ?? field.name,
            GeneratorTypeMap{field.type} ?? AbstractDataProviderTypeMap."string");
    }
}
}
