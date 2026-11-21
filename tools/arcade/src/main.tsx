import { render } from "preact";
import { App } from "./app";
import { sources } from "./sources";
import { createState, loadModel } from "./state";
import "./styles.css";
import { applyTheme, storedTheme } from "./ui/theme";

applyTheme(storedTheme());

const model = loadModel(sources);
render(<App model={model} state={createState(model)} />, document.getElementById("app")!);
