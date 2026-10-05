# Voice pack builder (optional)

`build_cori_pack.py` regenerates the Cori beta voice pack that ships in
`port/assets/voices/cori/`. You do not need it to build or play: the pack is
already in the repository and every build copies it next to the game as
`voices/`.

Needs a Piper venv:

    python3 -m venv venv && venv/bin/pip install piper-tts==1.8.0

and the model `en_GB-cori-high.onnx` (+ `.onnx.json`) from the official
`rhasspy/piper-voices` Hugging Face repo (`en/en_GB/cori/high/`), in
`tools/voices/models/`. Check the model's SHA256 against `notes/tts-research.md`
(470b4dd634c98f8a4850d7626ffc3dfc90774628eeef6605a6dd8f88f30a5903).

    venv/bin/python tools/voices/build_cori_pack.py /tmp/pack_cori

`picks_cori/` holds hand-picked takes that replace generated clips of the
same name; `beeps/sets/` holds the beeps. The model, the venv and `models/`
are not committed.
