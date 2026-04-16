# Forensic Cost Tracking Design

## Problem

We need to answer: "What was the true cost of this insight?" This isn't just summing token counts — it requires understanding the dependency graph of agent interactions and attributing costs along causal chains with fanout.

## Mental Model

An agent session is a **tree of interactions**, not a chain:

```
User message M1
├── LLM call L1 (cost: $0.05)
│   ├── Tool: search_entities (duration: 120ms)
│   ├── Tool: get_definition (duration: 80ms)
│   └── Tool: create_task (duration: 50ms)
├── LLM call L2 (cost: $0.08)  ← includes L1's tool results as context
│   ├── Tool: get_callers (depth=3) (duration: 2100ms)
│   ├── Tool: run_python (duration: 5400ms, $0.00 — local compute)
│   └── Tool: finish (→ insight I1)
├── Recommender R1 (cost: $0.01) ← background, triggered by L2 completion
└── Observer O1 (cost: $0.03) ← triggered every 5 tool calls
```

The **true cost of insight I1** = cost(L1) + cost(L2) + tools(L1) + tools(L2) + share(R1) + share(O1). But if L1 also produced insight I0 on a different branch, L1's cost is shared.

## Data Model

### Interaction Graph

Every LLM call, tool execution, and agent action is a node in a DAG:

```sql
CREATE TABLE gui_cost_nodes (
  node_id INTEGER PRIMARY KEY AUTOINCREMENT,
  session_id INTEGER NOT NULL,
  parent_node_id INTEGER,       -- NULL for root (user message)
  node_type TEXT NOT NULL,       -- 'user_message', 'llm_call', 'tool_call',
                                 -- 'recommender', 'observer', 'summarizer'
  
  -- Identity
  tool_name TEXT,                -- for tool_call nodes
  model TEXT,                    -- for llm_call nodes
  
  -- Cost
  input_tokens INTEGER DEFAULT 0,
  output_tokens INTEGER DEFAULT 0,
  duration_ms INTEGER DEFAULT 0,
  cost_usd REAL DEFAULT 0.0,    -- computed: (input * rate_in + output * rate_out)
  
  -- Timing
  started_at TEXT NOT NULL,
  completed_at TEXT,
  
  -- Content references
  message_id INTEGER,           -- FK to gui_agent_messages if applicable
  
  -- Metadata
  metadata TEXT                  -- JSON blob for tool-specific data
);

-- Edges in the dependency graph: which nodes depend on which
CREATE TABLE gui_cost_edges (
  edge_id INTEGER PRIMARY KEY AUTOINCREMENT,
  from_node_id INTEGER NOT NULL,  -- producer
  to_node_id INTEGER NOT NULL,    -- consumer
  edge_type TEXT NOT NULL         -- 'causal', 'context', 'trigger'
);
```

### Edge Types

- **causal**: A directly causes B (user message → LLM call → tool call)
- **context**: B's prompt includes A's output (previous tool results in LLM context window)
- **trigger**: A's completion triggers B (session complete → recommender fires)

### Cost Attribution

For any insight (a completed task, a finding document, a finish result):

1. **Direct cost**: sum of all nodes on the causal path from root to insight
2. **True cost**: direct cost + amortized cost of sibling branches explored along the way
3. **Marginal cost**: what would NOT have been spent if this branch was pruned

Algorithm:
```
true_cost(insight_node):
  path = trace_path_to_root(insight_node)
  cost = 0
  for node in path:
    # This node's direct cost
    cost += node.cost_usd
    # Plus all siblings at this level (the fanout we had to do)
    siblings = children(node.parent) - {node}
    cost += sum(subtree_cost(s) for s in siblings) / len(siblings + 1)
  return cost
```

### Per-Model Cost Rates

```sql
CREATE TABLE gui_cost_rates (
  model TEXT PRIMARY KEY,
  input_per_million REAL NOT NULL,   -- $/1M input tokens
  output_per_million REAL NOT NULL,  -- $/1M output tokens
  updated_at TEXT NOT NULL
);

-- Seed with known rates
INSERT INTO gui_cost_rates VALUES
  ('claude-sonnet-4-20250514', 3.0, 15.0, '2025-05-14'),
  ('claude-opus-4-20250514', 15.0, 75.0, '2025-05-14'),
  ('claude-haiku-4-5-20251001', 0.80, 4.0, '2025-10-01'),
  ('gpt-4o', 2.5, 10.0, '2025-01-01'),
  ('gpt-4o-mini', 0.15, 0.60, '2025-01-01');
```

## Recording

### When to create nodes

1. **User sends message** → create `user_message` node (root, no cost)
2. **LLM call starts** → create `llm_call` node, parent = last user_message or previous llm_call
3. **LLM call completes** → update with tokens, compute cost from rates
4. **Tool call starts** → create `tool_call` node, parent = current llm_call
5. **Tool call completes** → update with duration_ms
6. **Recommender fires** → create `recommender` node, edge type = trigger from session completion
7. **Observer fires** → create `observer` node, edge type = trigger
8. **Code summarizer fires** → create `summarizer` node, edge type = trigger

### Context edges

When building the LLM message array (buildMessages), every tool_result that goes into the context window creates a **context edge** from the tool_call node to the current llm_call node. This tracks "this LLM call's quality depended on these prior tool results."

## Querying

### "How much did this session cost?"
```sql
SELECT SUM(cost_usd) FROM gui_cost_nodes WHERE session_id = ?;
```

### "What was the most expensive tool?"
```sql
SELECT tool_name, COUNT(*) as calls, SUM(cost_usd) as total_cost,
       AVG(duration_ms) as avg_duration
FROM gui_cost_nodes 
WHERE session_id = ? AND node_type = 'tool_call'
GROUP BY tool_name ORDER BY total_cost DESC;
```

### "What was the true cost of this insight?"
Trace the DAG from the insight's node back to the root, accumulating costs including sibling fanout (requires recursive CTE or application-level traversal).

### "Which LLM calls were wasted?" (produced no useful downstream output)
```sql
SELECT n.node_id, n.cost_usd, n.output_tokens
FROM gui_cost_nodes n
LEFT JOIN gui_cost_edges e ON n.node_id = e.from_node_id
WHERE n.node_type = 'llm_call' AND e.edge_id IS NULL
  AND n.session_id = ?;
```
(LLM calls with no outgoing edges = dead ends)

### "Cost breakdown by role"
```sql
SELECT node_type, model, COUNT(*) as calls, 
       SUM(input_tokens) as total_in, SUM(output_tokens) as total_out,
       SUM(cost_usd) as total_cost
FROM gui_cost_nodes WHERE session_id = ?
GROUP BY node_type, model;
```

## What We Can Learn

1. **Tool efficiency**: which tools provide the most value per dollar? If `get_callers` costs $0.001 but saves 3 LLM calls ($0.15), it's 150x ROI.

2. **Model selection ROI**: does using Opus for the primary agent vs. Sonnet produce better insights per dollar? Track insight quality (user accepts/rejects suggestions, completes/abandons tasks) vs. cost.

3. **Exploration waste**: how much is spent on branches that lead nowhere? High waste = the system prompt or tool selection needs tuning.

4. **Optimal depth**: at what call hierarchy depth does the marginal cost exceed the marginal value? This informs default depth parameters.

5. **Recommender value**: does the recommender's suggestions lead to productive branches? Or are they ignored (wasted cost)?

6. **Observer impact**: does observer feedback improve subsequent agent behavior? Compare session quality before/after observer recommendations.

7. **Cost prediction**: given the current conversation state and task board, predict the remaining cost to completion. Use Bayesian estimation (like double-double's NIG model) based on historical sessions.

## UI

### Cost Dashboard (future)
- Session cost breakdown: pie chart by role (primary, recommender, observer, summarizer)
- Cost timeline: line chart of cumulative cost over the session
- Tool cost table: ranked by total cost, with call count and avg duration
- Insight cost attribution: for each completed task, show true cost

### Inline in conversation
- Each message bubble shows its direct cost: "$0.05 (1.2k in / 340 out)"
- Tool call cards show duration + cost
- The token counter already shows cumulative cost; enhance with role breakdown tooltip

## Implementation Order

1. **Phase 1**: Schema + recording (gui_cost_nodes table, create nodes in AgentSession)
2. **Phase 2**: Cost computation (rates table, automatic USD calculation)
3. **Phase 3**: Basic queries (session cost, tool efficiency, per-role breakdown)
4. **Phase 4**: Dependency edges (context tracking in buildMessages)
5. **Phase 5**: Cost attribution algorithm (true cost with fanout)
6. **Phase 6**: UI dashboard
7. **Phase 7**: Cost prediction (Bayesian estimation)
