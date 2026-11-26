# Adds the target uploadcard to the firmware's build, so python -m platformio run -t uploadcard
# copies firmware/src/resources/ and games/ onto the memory card in the Teensy with card-upload.py.
import os

Import("env")

_SCRIPT = os.path.join(env.subst("$PROJECT_DIR"), "firmware", "tools", "card-upload.py")

env.AddCustomTarget(
    name="uploadcard",
    dependencies=None,
    actions=[f'"$PYTHONEXE" "{_SCRIPT}"'],
    title="Upload card",
    description="Copies firmware/src/resources/ and games/ onto the memory card over USB",
)
