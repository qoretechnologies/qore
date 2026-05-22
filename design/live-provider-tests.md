# Live-API Tests for AI Provider DataProviders

This document covers how to run the live-API verification tests for the
AI provider modules added in Phases 1–5 of the AI expansion work.

## How it works

Each provider `.qtest` file accepts a `-c,--connection=ARG` option (and
falls back to a per-provider `XXX_CONNECTION` environment variable).
The argument names a Qore connection registered via
`ConnectionProvider`.  Without `-c` and without the env var, the live
test cases are skipped and the test runs purely against an offline
fake (fast, deterministic, $0).

Each live test exercises the cheapest available action on the cheapest
model that proves the wire format matches production.  Total cost per
full live sweep stays under $1 even when no provider is on a free
tier.

## Self-hostable providers (5)

For these you can spin up a podman container locally and create a
connection that points at it — no cloud account, no credit card.

### Ollama (LLM runtime)

```bash
podman run -d --name ollama -p 11434:11434 ollama/ollama
podman exec ollama ollama pull phi3:mini    # ~2.3 GB; smallest decent model
```

Create a Qore connection named `ollama-local`:

```sh
qore -X 'ConnectionProvider::register_connection("ollama-local",
    {"url": "ollama://localhost:11434"})'
```

Run:

```sh
examples/test/qlib/OllamaDataProvider/OllamaDataProvider.qtest -c ollama-local
```

### Chroma (vector store)

```bash
podman run -d --name chroma -p 8000:8000 chromadb/chroma:latest
```

Pre-seed an empty collection:

```bash
curl -s -XPOST http://localhost:8000/api/v2/tenants/default_tenant/databases/default_database/collections \
    -H 'Content-Type: application/json' \
    -d '{"name": "test", "dimension": 384}'
```

Create connection `chroma-local`; export
`CHROMA_COLLECTION_ID=<the-uuid-returned>`; run the qtest.

### Weaviate (vector store)

```bash
podman run -d --name weaviate -p 8080:8080 -p 50051:50051 \
    -e AUTHENTICATION_ANONYMOUS_ACCESS_ENABLED=true \
    -e DEFAULT_VECTORIZER_MODULE=none \
    cr.weaviate.io/semitechnologies/weaviate:1.27.0
```

Pre-create class `Test` via the schema API, then export
`WEAVIATE_CLASS=Test`, create connection `weaviate-local`, run qtest.

### Milvus (vector store)

```bash
curl -s https://raw.githubusercontent.com/milvus-io/milvus/master/scripts/standalone_embed.sh -o milvus.sh
bash milvus.sh start
# Milvus listens on localhost:19530
```

Create a small test collection (see Milvus quickstart), export
`MILVUS_COLLECTION=test`, create connection `milvus-local`, run qtest.

### pgvector (Postgres extension)

```bash
podman run -d --name pgvector -p 5432:5432 \
    -e POSTGRES_PASSWORD=test \
    pgvector/pgvector:pg17
psql -h localhost -U postgres -c "CREATE EXTENSION vector;"
psql -h localhost -U postgres -c "
    CREATE TABLE rag (id bigserial PRIMARY KEY,
                      content text, embedding vector(384));"
```

Create a Qore `pgsql` connection named `pgvector-local`, export
`PGVECTOR_TABLE=rag`, run qtest.

## Cloud-only providers (16)

These have no API-compatible self-hosted option — testing them
requires real cloud credentials.

| Provider | Env var(s) | Free tier / cost |
|---|---|---|
| AWS Bedrock | `BEDROCK_CONNECTION` | Pay-per-token; Claude Haiku live test ~$0.001/run |
| Azure OpenAI | `AZURE_OPENAI_CONNECTION` + `AZURE_OPENAI_DEPLOYMENT` | Pay-per-token; gpt-4o-mini live test ~$0.0001/run |
| Google Vertex AI | `VERTEX_AI_CONNECTION` | gemini-2.5-flash-lite is free-tier eligible |
| DeepSeek | `DEEPSEEK_CONNECTION` | Pay-as-you-go; live test ~$0.0001/run |
| Groq | `GROQ_CONNECTION` | Free tier covers full live test |
| Together AI | `TOGETHER_CONNECTION` | Pay-per-token; live test ~$0.00006/run |
| Fireworks AI | `FIREWORKS_CONNECTION` | Pay-per-token; live test ~$0.0001/run |
| Jina AI | `JINA_CONNECTION` | 1M tokens/month free |
| Cohere | `COHERE_CONNECTION` | 1000 trial calls/month free |
| Mistral AI | `MISTRAL_CONNECTION` | Pay-as-you-go; live test ~$0.0001/run |
| Pinecone | `PINECONE_CONNECTION` + `PINECONE_INDEX` | Serverless free tier covers live test |
| MongoDB Atlas | `MONGODB_ATLAS_CONNECTION` + `MONGODB_ATLAS_COLLECTION` | M0 free tier covers live test |
| Tavily | `TAVILY_CONNECTION` | 1000 searches/month free |
| Exa | `EXA_CONNECTION` | $10 trial credit (~2000 searches) |
| LlamaParse | `LLAMAPARSE_CONNECTION` | 1000 pages/day free |
| Unstructured | `UNSTRUCTURED_CONNECTION` | 1000 pages/month free |

For Phase 5 speech / vision / document-AI providers see also:

| Provider | Env var(s) | Free tier / cost |
|---|---|---|
| Deepgram | `DEEPGRAM_CONNECTION` | $200 free trial credit |
| AssemblyAI | `ASSEMBLYAI_CONNECTION` | Pay-per-second; nano model ~$0.001/run |
| AWS Textract | `AWS_TEXTRACT_CONNECTION` | 1000 pages/month free (12 mo) |
| Azure Document Intelligence | `AZURE_DOCUMENTINTELLIGENCE_CONNECTION` | 500 pages/month free (12 mo) |
| Google Document AI | `GOOGLE_DOCUMENTAI_CONNECTION` + `GOOGLE_DOCUMENTAI_PROCESSOR_ID` | 1000 pages/month free |
| Stability AI | `STABILITY_AI_CONNECTION` | $10 starter credit |
| Black Forest Labs | `BFL_CONNECTION` | Pay-per-image; flux-dev ~$0.025/run |

## Running the full live sweep

```bash
for t in examples/test/qlib/*DataProvider/*.qtest \
         examples/test/qlib/QoreRagUtils/PgVectorRagProvider.qtest \
         examples/test/qlib/MongoDbDataProvider/MongoAtlasVectorStore.qtest; do
    "$t" -v
done
```

Tests without a `-c` and no env var will silently skip their live
cases; tests with one or the other configured will run the live
verification.  Total runtime for the full sweep is a few minutes at
most; total cost is well under $1 if every provider is configured.

## Recommended bare-minimum setup for CI

The cheapest combination that exercises a real wire path for every
provider category:

- **One LLM**: Groq (free tier)
- **One embedder**: Jina (free tier)
- **One reranker**: Cohere (trial)
- **One vector store**: a local pgvector container
- **One web search**: Tavily (free tier)
- **One ingester**: Unstructured (free tier)
- **One speech**: AssemblyAI (nano model, ~$0.001)
- **One document AI**: AWS Textract (free tier)
- **One image gen**: BFL (flux-dev, ~$0.025)

Total: about $0.03 per CI run, exercising every architectural path.
