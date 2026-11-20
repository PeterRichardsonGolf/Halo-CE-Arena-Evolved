/*
AE_HUD_STYLE.H

HUD STYLE (display.hud_style, Settings > HUD's first row): "ae" draws the
score strip and Arena Evolved's look for everything AE draws on the HUD;
"classic" draws the HUD exactly as before. AE is the default for new and
upgraded installs: anything but "classic" reads as AE. Display only, this
machine's: nothing is sent.
*/

#ifndef __AE_HUD_STYLE_H
#define __AE_HUD_STYLE_H

#include "ae_pure_types.h"

/* TRUE unless display.hud_style is "classic" */
boolean ae_hud_style_is_ae(void);
/* "ae" or "classic", for logs */
const char *ae_hud_style_name(void);

#endif /* __AE_HUD_STYLE_H */
