// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: Apache-2.0
#include <arrow/api.h>
#include <arrow/acero/api.h>
#include <arrow/compute/api.h>
#include <arrow/compute/initialize.h>
#include <arrow/dataset/api.h>
#include <arrow/flight/api.h>
#include <arrow/flight/sql/client.h>
#include <arrow/flight/sql/server.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

static void check(const arrow::Status& status) {
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
}

template <typename T>
static T value(arrow::Result<T> result) {
    check(result.status());
    return std::move(result).ValueOrDie();
}

static void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// A loopback-only fixture uses the installed Flight and Flight SQL libraries.
class FixtureServer final : public arrow::flight::sql::FlightSqlServerBase {
public:
    explicit FixtureServer(std::shared_ptr<arrow::Table> table) : table_(std::move(table)) {}

    arrow::Result<std::unique_ptr<arrow::flight::FlightDataStream>> DoGetStatement(
            const arrow::flight::ServerCallContext&,
            const arrow::flight::sql::StatementQueryTicket& ticket) override {
        if (ticket.statement_handle != "table") {
            return arrow::Status::Invalid("unknown fixture ticket");
        }
        auto reader = std::make_shared<arrow::TableBatchReader>(table_);
        return std::make_unique<arrow::flight::RecordBatchStream>(reader);
    }

private:
    const std::shared_ptr<arrow::Table> table_;
};

// Always shut down and join, including when a client assertion throws.
class RunningServer {
public:
    explicit RunningServer(std::shared_ptr<arrow::Table> table) : server_(std::move(table)) {
        auto location = value(arrow::flight::Location::ForGrpcTcp("127.0.0.1", 0));
        check(server_.Init(arrow::flight::FlightServerOptions(location)));
        thread_ = std::jthread([this] { serve_status_ = server_.Serve(); });
    }

    ~RunningServer() {
        if (thread_.joinable()) {
            const auto status = server_.Shutdown();
            if (!status.ok()) {
                std::cerr << "Server shutdown: " << status.ToString() << '\n';
            }
            thread_.join();
        }
    }

    int port() const { return server_.port(); }

    void close() {
        check(server_.Shutdown());
        thread_.join();
        check(serve_status_);
    }

private:
    FixtureServer server_;
    arrow::Status serve_status_;
    std::jthread thread_;
};

static void test_sdk() {
    check(arrow::compute::Initialize());
    arrow::Int64Builder builder;
    check(builder.AppendValues({1, 2, 3}));
    check(builder.AppendNull());
    auto array = value(builder.Finish());
    auto table = arrow::Table::Make(arrow::schema({arrow::field("value", arrow::int64())}), {array});
    auto sum = value(arrow::compute::CallFunction("sum", {array}));
    require(sum.scalar_as<arrow::Int64Scalar>().value == 6, "compute sum mismatch");
    require(!arrow::compute::CallFunction("nonexistent_sdk_test_function", {array}).ok(),
        "unknown compute function accepted");

    auto ipc_output = value(arrow::io::BufferOutputStream::Create());
    auto writer = value(arrow::ipc::MakeStreamWriter(ipc_output, table->schema()));
    check(writer->WriteTable(*table));
    check(writer->Close());
    auto ipc_input = std::make_shared<arrow::io::BufferReader>(value(ipc_output->Finish()));
    auto ipc_reader = value(arrow::ipc::RecordBatchStreamReader::Open(ipc_input));
    require(value(ipc_reader->ToTable())->Equals(*table), "IPC roundtrip mismatch");

    auto parquet_output = value(arrow::io::BufferOutputStream::Create());
    check(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), parquet_output, 2));
    auto parquet_input = std::make_shared<arrow::io::BufferReader>(value(parquet_output->Finish()));
    auto parquet_reader = value(parquet::arrow::OpenFile(parquet_input, arrow::default_memory_pool()));
    auto restored = value(parquet_reader->ReadTable());
    require(restored->Equals(*table), "Parquet roundtrip mismatch");

    arrow::acero::Declaration source("table_source", arrow::acero::TableSourceNodeOptions(table));
    require(value(arrow::acero::DeclarationToTable(std::move(source)))->Equals(*table),
        "Acero source mismatch");
    auto dataset = std::make_shared<arrow::dataset::InMemoryDataset>(table);
    auto scanner_builder = value(dataset->NewScan());
    auto scanner = value(scanner_builder->Finish());
    require(value(scanner->ToTable())->Equals(*table), "dataset scan mismatch");

    RunningServer server(table);
    auto location = value(arrow::flight::Location::ForGrpcTcp("127.0.0.1", server.port()));
    std::shared_ptr<arrow::flight::FlightClient> client = value(arrow::flight::FlightClient::Connect(location));
    arrow::flight::FlightCallOptions options;
    options.timeout = std::chrono::seconds(5);
    auto ticket = value(arrow::flight::sql::CreateStatementQueryTicket("table"));
    auto stream = value(client->DoGet(options, arrow::flight::Ticket(ticket)));
    require(value(stream->ToTable())->Equals(*table), "Flight roundtrip mismatch");
    arrow::flight::sql::FlightSqlClient sql(client);
    auto schema_result = value(sql.GetSqlInfoSchema(options));
    arrow::ipc::DictionaryMemo memo;
    require(value(schema_result->GetSchema(&memo))->num_fields() == 2,
        "Flight SQL metadata schema mismatch");
    check(client->Close());
    server.close();
}

int main() {
    try {
        test_sdk();
        std::cout << "PASS: installed Arrow, compute, IPC, Parquet, Acero, dataset, Flight and Flight SQL SDKs\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
