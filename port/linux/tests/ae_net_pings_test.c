#include <stdio.h>
#include <wchar.h>
#include "ae_net_pings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int setting = 1;
int ae_browser_pings_on(void) { return setting; }

static int same(const wchar_t *a, const wchar_t *b)
{
	return wcscmp(a, b) == 0;
}

int main(void)
{
	wchar_t text[8] = L"KEEP";
	float rgb[3] = { -1.0f, -1.0f, -1.0f };

	/* off: nothing shown, nothing touched (the rows' own text stands) */
	CHECK(ae_browser_ping_shown(0, 40, 0) == -1);
	CHECK(!ae_browser_ping_text_of(0, 40, 0, text, 8) && same(text, L"KEEP"));
	CHECK(!ae_browser_ping_rgb_of(0, 40, 0, rgb) && rgb[0] < 0.0f);
	setting = 0;
	CHECK(!ae_browser_ping_text(40, 0, text, 8) && same(text, L"KEEP"));
	setting = 1;
	/* on: a known ping shown; none known, or a failed or closed game: not */
	CHECK(ae_browser_ping_shown(1, 40, 0) == 40);
	CHECK(ae_browser_ping_shown(1, -1, 0) == -1);
	CHECK(ae_browser_ping_shown(1, 40, 1) == -1);
	CHECK(ae_browser_ping_text_of(1, 0, 0, text, 8) && same(text, L"0"));
	CHECK(ae_browser_ping_text_of(1, 40, 0, text, 8) && same(text, L"40"));
	CHECK(ae_browser_ping_text_of(1, 1234, 0, text, 8) && same(text, L"1234"));
	CHECK(ae_browser_ping_text_of(1, 7654321, 0, text, 8) && same(text, L"99999"));
	CHECK(ae_browser_ping_text(85, 0, text, 8) && same(text, L"85"));
	/* (a short buffer is cut, never overrun) */
	{
		wchar_t small[3] = { 'x', 'x', 'x' };

		CHECK(ae_browser_ping_text_of(1, 1234, 0, small, 3) && small[0] == '1' && small[1] == '2' && small[2] == 0);
		CHECK(!ae_browser_ping_text_of(1, 12, 0, small, 1));
	}
	/* the colours by DamnationCE's thresholds: green under 80, yellow under 160, red above */
	CHECK(ae_browser_ping_rgb_of(1, 79, 0, rgb) && rgb[0] < rgb[1] && rgb[1] > 0.8f);
	CHECK(ae_browser_ping_rgb_of(1, 80, 0, rgb) && rgb[0] > 0.9f && rgb[1] > 0.7f && rgb[2] < 0.3f);
	CHECK(ae_browser_ping_rgb_of(1, 159, 0, rgb) && rgb[0] > 0.9f && rgb[1] > 0.7f);
	CHECK(ae_browser_ping_rgb_of(1, 160, 0, rgb) && rgb[0] > 0.9f && rgb[1] < 0.5f);
	CHECK(!ae_browser_ping_rgb_of(1, 40, 1, rgb));
	/* REFRESH: a host never probed, or probed 30 s ago or more, is timed again; not one probed sooner */
	CHECK(ae_browser_probe_refresh_due(0, 5000));
	CHECK(!ae_browser_probe_refresh_due(1001, 1001 + 29999));
	CHECK(ae_browser_probe_refresh_due(1001, 1001 + 30000));
	/* (across the clock's wrap) */
	CHECK(!ae_browser_probe_refresh_due((unsigned long)-1000, 5000));
	CHECK(ae_browser_probe_refresh_due((unsigned long)-1000, 29000));

	if (failures)
		return 1;
	printf("ae_net_pings_test: all passed\n");
	return 0;
}
