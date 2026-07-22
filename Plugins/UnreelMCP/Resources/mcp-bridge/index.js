#!/usr/bin/env node

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

const CONFIG = {
  baseUrl: process.env.UNREEL_MCP_URL || process.env.UNREAL_MCP_URL || "http://localhost:55570",
  timeoutMs: Number.parseInt(process.env.UNREEL_MCP_TIMEOUT_MS || "30000", 10),
};

async function fetchJson(path, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), CONFIG.timeoutMs);
  try {
    const response = await fetch(`${CONFIG.baseUrl}${path}`, {
      ...options,
      signal: controller.signal,
      headers: {
        "content-type": "application/json",
        ...(options.headers || {}),
      },
    });
    const text = await response.text();
    let json = {};
    if (text) {
      json = JSON.parse(text);
    }
    if (!response.ok) {
      throw new Error(json.message || `HTTP ${response.status}`);
    }
    return json;
  } finally {
    clearTimeout(timeout);
  }
}

function asText(payload) {
  return {
    content: [
      {
        type: "text",
        text: typeof payload === "string" ? payload : JSON.stringify(payload, null, 2),
      },
    ],
  };
}

function asError(error) {
  return {
    content: [{ type: "text", text: error instanceof Error ? error.message : String(error) }],
    isError: true,
  };
}

async function postJson(path, body) {
  return fetchJson(path, {
    method: "POST",
    body: JSON.stringify(body || {}),
  });
}

const server = new Server(
  {
    name: "unreel-mcp-bridge",
    version: "0.1.0",
  },
  {
    capabilities: {
      tools: {},
    },
  }
);

server.setRequestHandler(ListToolsRequestSchema, async () => ({
  tools: [
    {
      name: "unreal_status",
      description: "Check whether UnreelMCP is reachable inside Unreal Editor.",
      inputSchema: { type: "object", properties: {} },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    {
      name: "unreal_tool_search",
      description: "Search UnreelMCP service operations by query and optional domain.",
      inputSchema: {
        type: "object",
        properties: {
          query: { type: "string", description: "Search terms such as actor, asset, blueprint, viewport, or log." },
          domain: { type: "string", description: "Optional exact domain filter." },
        },
      },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    {
      name: "unreal_tool_describe",
      description: "Describe one UnreelMCP operation, including parameters, risk, and example JSON.",
      inputSchema: {
        type: "object",
        required: ["operation_id"],
        properties: {
          operation_id: { type: "string", description: "Operation id returned by unreal_tool_search." },
        },
      },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    {
      name: "unreal_call",
      description: "Call an UnreelMCP operation by id with JSON params.",
      inputSchema: {
        type: "object",
        required: ["operation_id"],
        properties: {
          operation_id: { type: "string", description: "Operation id returned by search/describe." },
          params: { type: "object", description: "Operation-specific parameters." },
        },
      },
      annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
    },
    {
      name: "unreal_context",
      description: "Get concise UnreelMCP usage context for an MCP client.",
      inputSchema: { type: "object", properties: {} },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
    {
      name: "unreal_transaction",
      description: "Inspect UnreelMCP transaction policy. Mutating calls are expected to create Unreal transactions.",
      inputSchema: { type: "object", properties: {} },
      annotations: { readOnlyHint: true, destructiveHint: false, idempotentHint: true, openWorldHint: false },
    },
  ],
}));

server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;
  try {
    if (name === "unreal_status") {
      return asText(await fetchJson("/mcp/status"));
    }
    if (name === "unreal_tool_search") {
      return asText(await postJson("/mcp/search", args || {}));
    }
    if (name === "unreal_tool_describe") {
      return asText(await postJson("/mcp/describe", args || {}));
    }
    if (name === "unreal_call") {
      return asText(await postJson("/mcp/call", args || {}));
    }
    if (name === "unreal_context") {
      return asText(await postJson("/mcp/context", args || {}));
    }
    if (name === "unreal_transaction") {
      return asText(await postJson("/mcp/call", { operation_id: "transaction.status", params: args || {} }));
    }
    return asError(`Unknown tool: ${name}`);
  } catch (error) {
    return asError(error);
  }
});

const transport = new StdioServerTransport();
await server.connect(transport);
