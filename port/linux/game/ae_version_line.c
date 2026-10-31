/*
AE_VERSION_LINE.C

The build's version as the performance line's tail (ae_version_line.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/ui_widget.h"
#include "ae_version_line.h"

extern int config_boolean(char const *name);
extern char const *build_identity_version(void);

int ae_version_line_takes(void)
{
	return config_boolean("display.version_watermark") && !main_menu_is_active() && !ui_widgets_active();
}

int ae_version_line_append(char *line, long size)
{
	size_t length;
	int alone;

	if (!line || size <= 0 || !ae_version_line_takes())
		return 0;
	length = strlen(line);
	alone = length == 0;
	/* (after the line's last group, two spaces apart as the groups are; a second line's end, if one is there) */
	snprintf(line + length, (size_t)size - length, "%sv%s", alone ? "" : "  ", build_identity_version());
	return alone;
}

void ae_version_line_color(union real_argb_color *color)
{
	color->alpha = 0.4f;
	color->red = 0.8f;
	color->green = 0.8f;
	color->blue = 0.8f;
}
