import { renderToStaticMarkup } from "react-dom/server";
import ts from "typescript";
import { describe, expect, it } from "vitest";
import App from "./App";
import { BundleComposer } from "./gestures/BundleComposer";

const components = import.meta.glob<string>(["./**/*.tsx", "!./**/*.test.tsx"], {
  eager: true,
  query: "?raw",
  import: "default",
});

describe("human-readable interface text", () => {
  it.each(Object.entries(components))("%s has no literal escape sequences in JSX text", (path, source) => {
    const file = ts.createSourceFile(path, source, ts.ScriptTarget.Latest, true, ts.ScriptKind.TSX);
    function visit(node: ts.Node) {
      if (ts.isJsxText(node)) {
        expect(node.text, `Unrendered escape in ${path}`).not.toMatch(/\\(?:u[\da-fA-F]{4}|u\{[\da-fA-F]+\}|[nrt])/);
      }
      ts.forEachChild(node, visit);
    }
    visit(file);
  });

  it("renders a real middle dot in the bundle summary", () => {
    const markup = renderToStaticMarkup(<BundleComposer onSave={() => {}} />);
    expect(markup).toContain("0 gestures · 0.0s");
    expect(markup).not.toContain("\\u00b7");
    expect(markup).toContain('aria-label="Bundle name"');
  });

  it("renders the disconnected desk without a native device or made-up connection", () => {
    const markup = renderToStaticMarkup(<App />);
    expect(markup).toContain("Pet control desk");
    expect(markup).toContain("is-disconnected");
    expect(markup).toContain("browser preview");
    expect(markup).toContain("Small pet.");
    expect(markup).not.toMatch(/\\u[\da-fA-F]{4}/);
  });
});
