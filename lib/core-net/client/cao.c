/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2022 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "private-lib-core.h"

lws_cao_t *
lws_cao_create(struct lws *wsi)
{
	lws_cao_t *cao;
	size_t s = sizeof(*cao);

#if defined(LWS_WITH_EVENT_LIBS)
	/* overallocate evlib priv area at CAO */
	s += wsi->a.context->event_loop_ops->evlib_size_wsi;
#endif

	cao = lws_zalloc(s, __func__);

	if (!cao)
		return NULL;

	lws_dll2_add_tail(&cao->wsi_list, &wsi->cao_owner);
	cao->desc.u.sockfd = LWS_SOCK_INVALID;
	cao->desc.pos_in_fds_table = LWS_NO_FDS_POS;
#if defined(LWS_WITH_EVENT_LIBS)
	cao->desc.evlib_desc = (void *)(cao + 1);
#endif

	return cao;
}

void
lws_cao_destroy_if_unreferenced(lws_cao_t **_cao)
{
	lws_cao_t *cao = *_cao;

	if (cao->wsi_list.owner)
		return;

	*_cao = NULL;
	lws_free(cao);
}

void
lws_cao_destroy(lws_cao_t **_cao)
{
	lws_cao_t *cao = *_cao;

	if (cao->desc.u.sockfd != LWS_SOCK_INVALID) {
		compatible_close(cao->desc.u.sockfd);
	}

	lws_dll2_remove(&cao->wsi_list);

	lws_cao_destroy_if_unreferenced(_cao);
}

void
lws_cao_remove_all(struct lws *wsi)
{
	lws_start_foreach_dll_safe(struct lws_dll2 *, d, d1,
				   lws_dll2_get_head(&wsi->cao_owner)) {
		lws_cao_t *cao = lws_container_of(d, lws_cao_t, wsi_list);

		lws_dll2_remove(&cao->wsi_list);
		lws_cao_destroy_if_unreferenced(&cao);

	} lws_end_foreach_dll_safe(d, d1);
}

/*
 * We call this internally when we understand the wsi to be a live client wsi
 * already, and so expect it to have a cao associated with it.
 *
 * For simple client wsi, the CAO(s) are on the wsi.  For muxed wsi, the CAOs
 * are on the nwsi of the mux parent.
 */

lws_cao_t *
_lws_wsi_cao(struct lws *wsi)
{
	lws_dll2_t *head;

#if defined(LWS_ROLE_H2) || defined(LWS_ROLE_MQTT)
	if (wsi->mux.parent_wsi)
		wsi = wsi->mux.parent_wsi;
#endif

	wsi = lws_get_network_wsi(wsi);
	head = lws_dll2_get_head(&wsi->cao_owner);

	if (!head && wsi->parent)
		head = lws_dll2_get_head(&wsi->parent->cao_owner);

	if (!head) {
		lwsl_wsi_err(wsi, "no caos");
		return NULL;
	}

	return lws_container_of(head, lws_cao_t, wsi_list);
}

lws_cao_t *
lws_wsi_cao(struct lws *wsi)
{
	lws_cao_t *cao = _lws_wsi_cao(wsi);

	if (cao)
		return cao;

	lwsl_wsi_warn(wsi, "No head cao");
	assert(0);

	return NULL;
}

lws_conmon_t *
lws_wsi_conmon(struct lws *wsi)
{
	lws_cao_t *cao = lws_wsi_cao(wsi);

	if (!cao)
		return NULL;

	return &cao->conmon;
}
