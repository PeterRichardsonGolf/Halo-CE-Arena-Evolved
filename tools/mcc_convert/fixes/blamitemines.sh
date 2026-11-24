# empty sound_scenery_palette[3] (unused by instances): erase it from the build copy and from the gate's source copy,
# so the gate compares the palettes with that one unused entry removed on both sides. The erase renumbers no placed
# instance, so it is made only when [3] is the last entry and no sound scenery instance has type 3.
scn="levels/femix/blamitemines/blamitemines mcc flycam pm.scenario"
dump=$("$I/invader-edit" -t tags2 -L "$scn")
last=$(printf '%s\n' "$dump" | grep -a -c '^sound_scenery_palette\[4\] ')
used=$(printf '%s\n' "$dump" | awk '/^sound_scenery\[/{in_inst=1; next} /^[^ ]/{in_inst=0} in_inst && /type \(3\) /{n++} END{print n+0}')
if [ "$last" != 1 ] || [ "$used" != 0 ]; then
  echo "blamitemines: sound_scenery_palette[3] is not an unused last entry (palette of 4: $last, instances of type 3: $used)" >&2; exit 67
fi
"$I/invader-edit" -t tags2 -E "sound_scenery_palette[3]" "$scn"
"$I/invader-edit" -t tags_src -E "sound_scenery_palette[3]" "$scn"
echo "mirrored: sound_scenery_palette[3] (empty, last, unused) erased in the gate's source copy too" >> fixes.log
