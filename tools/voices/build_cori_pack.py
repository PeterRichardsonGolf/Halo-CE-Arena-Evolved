"""Builds a callout voice pack with Piper (local, public-domain cori voice).
Clip names as notes/nhe-voice-timers.md / the callout player expect."""
import json, os, sys, wave
from piper import PiperVoice, SynthesisConfig
# usage: build_cori_pack.py <out folder> [model.onnx]; paths are relative to this file's folder
HERE = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]
v = PiperVoice.load(sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'models', 'en_GB-cori-high.onnx'))
nums = ['one','two','three','four','five','six','seven','eight','nine','ten']
clips = {n: n + '.' for n in nums}
clips['twenty_seconds'] = 'twenty seconds'
clips['thirty_seconds_left'] = 'thirty seconds [[ lˈɛftʰ ]]'
words = nums + ['eleven','twelve','thirteen','fourteen','fifteen','sixteen','seventeen','eighteen','nineteen','twenty',
	'twenty one','twenty two','twenty three','twenty four','twenty five','twenty six','twenty seven','twenty eight',
	'twenty nine','thirty']
names = ['one','two','three','four','five','six','seven','eight','nine','ten','eleven','twelve','thirteen','fourteen',
	'fifteen','sixteen','seventeen','eighteen','nineteen','twenty','twenty_one','twenty_two','twenty_three',
	'twenty_four','twenty_five','twenty_six','twenty_seven','twenty_eight','twenty_nine','thirty']
for i, w in enumerate(words):
	clips[names[i] + ('_minute' if i == 0 else '_minutes')] = w + (' minute.' if i == 0 else ' minutes.')
CAMO = 'cammo'
OS = '[[ ˈoʊvɚʃˌiːld ]]'
SLOW = {'camo_up': 1.1}
SLOW_TIMES = 1.45
clips.update({'rockets': 'rockets', 'red_rockets': 'red rockets', 'blue_rockets': 'blue rockets',
	'sniper': 'sniper', 'camo': CAMO, 'overshield': OS + ' in ten', 'shotgun': 'shotgun',
	'rockets_in_ten': 'rockets in ten', 'sniper_in_ten': 'sniper in ten', 'camo_in_ten': CAMO + ' in ten',
	'overshield_in_ten': OS + ' in ten', 'shotgun_in_ten': 'shotgun in ten',
	'rockets_up': 'rockets are up', 'sniper_up': 'sniper is up', 'camo_up': 'active ' + CAMO + ' is up',
	'overshield_up': OS + ' is up', 'shotgun_up': 'shotgun is up'})
for side in ('red', 'blue'):
	clips.update({side + '_sniper': side + ' sniper', side + '_camo': side + ' ' + CAMO, side + '_overshield': side + ' ' + OS,
		side + '_shotgun': side + ' shotgun'})
clips.update({'overshield_or_camo_in_ten': OS + ' or ' + CAMO + ' in ten', 'overshield_camo_in_ten': OS + ' and ' + CAMO + ' in ten seconds',
	'overshield_camo_up': OS + ' and ' + CAMO + ' are up', 'powerups_in_ten': 'powerups in ten'})
def clip_config(name):
	if name in nums:
		return SynthesisConfig(length_scale=1.15, noise_scale=0.4)
	if 'minute' in name:
		return SynthesisConfig(length_scale=1.55, noise_scale=0.4)
	if 'seconds' in name:
		return SynthesisConfig(length_scale=SLOW_TIMES)
	return SynthesisConfig(length_scale=SLOW.get(name, 1.0))

manifest = {}
import os
os.makedirs(out, exist_ok=True)
for name, text in clips.items():
	path = os.path.join(out, name + '.wav')
	with wave.open(path, 'wb') as w:
		v.synthesize_wav(text, w, syn_config=clip_config(name))
	with wave.open(path) as r:
		manifest[name] = {'text': text, 'rate': r.getframerate(), 'seconds': round(r.getnframes() / r.getframerate(), 3)}
json.dump({'voice': 'en_GB-cori-high (public domain, Piper)', 'clips': manifest}, open(os.path.join(out, 'manifest.json'), 'w'), indent=1)
print(len(manifest), 'clips')

# picked takes (chosen by ear) replace the generated ones: picks_cori/<clip>.wav
import shutil, glob
for pick in glob.glob(os.path.join(HERE, 'picks_cori', '*.wav')):
	shutil.copy(pick, os.path.join(out, os.path.basename(pick)))

# the beeps chosen by ear (beeps/): minute, :20/:30/:40 ticks, before an item call, and the default
for clip, source in (('beep_minute', 'sets/minute_double_pluck'), ('beep_tick', 'sets/tick_single_soft'),
		('beep_item', 'sets/item_rising_chime'), ('beep', 'sets/tick_single_soft')):
	shutil.copy(os.path.join(HERE, 'beeps', source + '.wav'), os.path.join(out, clip + '.wav'))
