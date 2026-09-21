import { MAX_SCROLLING_TEXT_LENGTH, scrollingTextError } from "./scrollingText";

type Props = {
  id: string;
  value: string;
  onChange: (value: string) => void;
};

export function ScrollingTextInput({ id, value, onChange }: Props) {
  const error = scrollingTextError(value);
  return (
    <div className="scrolling-text-field">
      <label htmlFor={id}>Scrolling text</label>
      <input
        id={id}
        type="text"
        value={value}
        onChange={(event) => onChange(event.target.value)}
        aria-invalid={!!error}
        aria-describedby={`${id}-hint${error ? ` ${id}-error` : ""}`}
      />
      <small id={`${id}-hint`}>{value.length}/{MAX_SCROLLING_TEXT_LENGTH} characters · Printable ASCII only</small>
      {error && <p id={`${id}-error`} className="scrolling-text-error" role="alert">{error}</p>}
    </div>
  );
}
