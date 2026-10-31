/*
AE_UPDATE_VERSION.H

Arena Evolved's order of two versions' suffixes for the updater (port/linux/src/updater.c's
updater_version_compare, an AE hook): included by updater.c in every build that compares versions, the updater's
pick test too (port/linux/tests/ae_updater_pick_test.c).
*/

#ifndef AE_UPDATE_VERSION_H
#define AE_UPDATE_VERSION_H

/* two versions' suffixes after their three numbers (updater_version_compare): character by character, but a run of
digits in both by its number, so that -rc10 comes after -rc9 and .10-beta after .9-beta (strcmp put them the other
way: a signed older release would have been offered as newer) */
static int ae_update_suffix_compare(const char *left, const char *right)
{
	while (*left && *right)
	{
		if (*left >= '0' && *left <= '9' && *right >= '0' && *right <= '9')
		{
			const char *left_end = left, *right_end = right;

			while (*left == '0' && left[1] >= '0' && left[1] <= '9')
				left++;
			while (*right == '0' && right[1] >= '0' && right[1] <= '9')
				right++;
			for (left_end = left; *left_end >= '0' && *left_end <= '9'; left_end++)
				;
			for (right_end = right; *right_end >= '0' && *right_end <= '9'; right_end++)
				;
			if (left_end - left != right_end - right)
				return left_end - left > right_end - right ? 1 : -1;
			for (; left < left_end; left++, right++)
				if (*left != *right)
					return *left > *right ? 1 : -1;
			continue;
		}
		if (*left != *right)
			return (unsigned char)*left > (unsigned char)*right ? 1 : -1;
		left++;
		right++;
	}
	return *left ? 1 : *right ? -1 : 0;
}

#endif
