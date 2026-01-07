//   Copyright 2022 Will Thomas
// 	 Modifications Copyright 2026 Olija Downing
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0;
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "qoidecode.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "libSRWWL.h"

struct wl_compositor* comp;
struct wl_surface* srfc;
struct wl_buffer* bfr;
struct wl_shm* shm;
struct xdg_wm_base* sh;
struct xdg_toplevel* top;
struct wl_seat* seat;
struct wl_keyboard* kb;
struct zxdg_decoration_manager_v1* deco_mgr;
struct zxdg_toplevel_decoration_v1* deco;
uint8_t* img;
void (*img_resize)();
uint8_t* pixl;
uint16_t w = 960;
uint16_t h = 540;
uint8_t cls;
bool SSD = true;
struct image minimise_button;
struct image close_button;
const int CSD_bar_size = 40;
bool frame_pending = false;
bool is_key_down[256];

int32_t alc_shm(uint64_t sz) {
	int8_t name[8];
	name[0] = '/';
	name[7] = 0;
	for (uint8_t i = 1; i < 6; i++) {
		name[i] = (rand() & 23) + 97;
	}

	int32_t fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR | S_IWOTH | S_IROTH);
	shm_unlink(name);
	ftruncate(fd, sz);

	return fd;
}

void resz() {

	img_resize();

	if (!SSD) h += CSD_bar_size;

	int32_t fd = alc_shm(w * h * 4);

	pixl = mmap(0, w * h * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool* pool = wl_shm_create_pool(shm, fd, w * h * 4);
	bfr = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (!SSD) h -= CSD_bar_size;
}

void set_col_a(uint8_t* v1, uint8_t v2, uint8_t a) {
	*v1 = (uint8_t)(((uint16_t)(*v1) * (255 - a) + (uint16_t)v2 * a) >> 8);
}

void drawCSD() {
	for (int y = 0; y < CSD_bar_size; y++) {
		for (int x = 0; x < w; x++) {
			size_t p = (size_t)(y * w + x) * 4;
			pixl[p + 0] = 127;
			pixl[p + 1] = 127;
			pixl[p + 2] = 127;
			pixl[p + 3] = 255;
		}
	}
    int x0 = w - 40, x1 = w - 20; // close button x-range
    int y0 = 10,     y1 = 30;     // close button y-range

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
			struct pixel pix_close = close_button.data[(y-y0) * close_button.w + (x-x0)];
            size_t p_close = (size_t)(y * w + x) * 4; // BGRA in memory (little-endian)
            set_col_a(&pixl[p_close + 0], pix_close.b, pix_close.a);   // B
            set_col_a(&pixl[p_close + 1], pix_close.g, pix_close.a);   // G
            set_col_a(&pixl[p_close + 2], pix_close.r, pix_close.a);   // R
            pixl[p_close + 3] = 255;		   						   // A
			struct pixel pix_minimise = minimise_button.data[(y-y0) * minimise_button.w + (x-x0)];
			size_t p_minimise = (size_t)(y * w + x - 40) * 4; // BGRA in memory (little-endian)
            set_col_a(&pixl[p_minimise + 0], pix_minimise.b, pix_minimise.a);   // B
            set_col_a(&pixl[p_minimise + 1], pix_minimise.g, pix_minimise.a);   // G
            set_col_a(&pixl[p_minimise + 2], pix_minimise.r, pix_minimise.a);   // R
            pixl[p_minimise + 3] = 255;				   			 				// A
        }
    }
}


void draw() {
	//memset(img, c, w * h * 4);
	if (SSD) {
		memcpy(pixl, img, w*h*4*sizeof(uint8_t));
	}
	else {
		memcpy(&pixl[w*CSD_bar_size*4*sizeof(uint8_t)], img, w*h*4*sizeof(uint8_t));
		drawCSD();
	}
	wl_surface_attach(srfc, bfr, 0, 0);
	wl_surface_damage_buffer(srfc, 0, 0, w, h);
	wl_surface_commit(srfc);
}

struct wl_callback_listener cb_list;

void frame_new(void* data, struct wl_callback* cb, uint32_t a) {
	frame_pending = false;
	
	wl_callback_destroy(cb);
	cb = wl_surface_frame(srfc);
	wl_callback_add_listener(cb, &cb_list, 0);
	
	//c++;
	draw();
}

struct wl_callback_listener cb_list = {
	.done = frame_new
};

void xrfc_conf(void* data, struct xdg_surface* xrfc, uint32_t ser) {
	xdg_surface_ack_configure(xrfc, ser);
	if (!pixl) {
		resz();
	}
	
	draw();
}

struct xdg_surface_listener xrfc_list = {
	.configure = xrfc_conf
};

void top_conf(void* data, struct xdg_toplevel* top, int32_t nw, int32_t nh, struct wl_array* stat) {
	return;
	//we don't want to be able to resize the window
	if (!nw && !nh) {
		return;
	}

	if (w != nw || h != nh) {
		munmap(pixl, w * h * 4);
		w = nw;
		h = nh;
		resz();
	}
}

void top_cls(void* data, struct xdg_toplevel* top) {
	cls = 1;
}

struct xdg_toplevel_listener top_list = {
	.configure = top_conf,
	.close = top_cls
};

void sh_ping(void* data, struct xdg_wm_base* sh, uint32_t ser) {
	xdg_wm_base_pong(sh, ser);
}

struct xdg_wm_base_listener sh_list = {
	.ping = sh_ping
};

void kb_map(void* data, struct wl_keyboard* kb, uint32_t frmt, int32_t fd, uint32_t sz) {
	
}

void kb_enter(void* data, struct wl_keyboard* kb, uint32_t ser, struct wl_surface* srfc, struct wl_array* keys) {
	
}

void kb_leave(void* data, struct wl_keyboard* kb, uint32_t ser, struct wl_surface* srfc) {
	
}

void kb_key(void* data, struct wl_keyboard* kb, uint32_t ser, uint32_t t, uint32_t key, uint32_t stat) {
	printf("%d\n", key);
	if (stat) is_key_down[key] = true;
	else is_key_down[key] = false;
	//if (key == 1) {
	//	cls = 1;
	//}
	//else if (key == 30) {
	//	printf("a\n");
	//}
	//else if (key == 32) {
	//	printf("d\n");
	//}
}

void kb_mod(void* data, struct wl_keyboard* kb, uint32_t ser, uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp) {
	
}

void kb_rep(void* data, struct wl_keyboard* kb, int32_t rate, int32_t del) {
	
}

int mx, my;

struct wl_keyboard_listener kb_list = {
	.keymap = kb_map,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_mod,
	.repeat_info = kb_rep
};

struct wl_pointer* ptr;

void start_drag(uint32_t serial) { xdg_toplevel_move(top, seat, serial); }

int in_close_button(int mx, int my) {
    return (mx >= w-40 && mx <= w-20 && my >= 10 && my <= 30);
}
int in_minimise_button(int mx, int my) {
    return (mx >= w-80 && mx <= w-60 && my >= 10 && my <= 30);
}

void ptr_enter(void* data, struct wl_pointer* ptr,
               uint32_t serial, struct wl_surface* surface,
               wl_fixed_t sx, wl_fixed_t sy) {}

void ptr_leave(void* data, struct wl_pointer* ptr,
               uint32_t serial, struct wl_surface* surface) {}

void ptr_motion(void* data, struct wl_pointer* ptr,
                uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    // track mouse position
    mx = wl_fixed_to_int(sx);
    my = wl_fixed_to_int(sy);
}

void ptr_button(void* data, struct wl_pointer* ptr,
                uint32_t serial, uint32_t time,
                uint32_t button, uint32_t state) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        // check if mouse position is inside your drawn "close" or "minimise" rectangles
        if (in_close_button(mx, my)) {
            cls = 1; // trigger close
        } else if (in_minimise_button(mx, my)) {
			xdg_toplevel_set_minimized(top);
        } else {
            // start drag if not in button
            start_drag(serial);
        }
    }
}

void ptr_axis(void* data, struct wl_pointer* ptr,
              uint32_t time, uint32_t axis, wl_fixed_t value) {}

struct wl_pointer_listener ptr_list = {
    .enter = ptr_enter,
    .leave = ptr_leave,
    .motion = ptr_motion,
    .button = ptr_button,
    .axis = ptr_axis
};

void seat_cap(void* data, struct wl_seat* seat, uint32_t cap) {
	if (cap & WL_SEAT_CAPABILITY_POINTER && !ptr) {
		ptr = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(ptr, &ptr_list, 0);
	}
	if (cap & WL_SEAT_CAPABILITY_KEYBOARD && !kb) {
		kb = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(kb, &kb_list, 0);
	}
}


void seat_name(void* data, struct wl_seat* seat, const char* name) {
		
}

struct wl_seat_listener seat_list = {
	.capabilities = seat_cap,
	.name = seat_name
};

void reg_glob(void* data, struct wl_registry* reg, uint32_t name, const char* intf, uint32_t v) {
    if (!strcmp(intf, wl_compositor_interface.name)) {
        comp = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    }
    else if (!strcmp(intf, wl_shm_interface.name)) {
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    }
    else if (!strcmp(intf, xdg_wm_base_interface.name)) {
        sh = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(sh, &sh_list, 0);
    }
    else if (!strcmp(intf, zxdg_decoration_manager_v1_interface.name)) {
        deco_mgr = wl_registry_bind(reg, name, &zxdg_decoration_manager_v1_interface, 1);
    }
    else if (!strcmp(intf, wl_seat_interface.name)) {
        seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seat_list, 0);
    }
}

void reg_glob_rem(void* data, struct wl_registry* reg, uint32_t name) {
	
}

struct wl_registry_listener reg_list = {
	.global = reg_glob,
	.global_remove = reg_glob_rem
};

void img_resz_def() {}

struct wl_display* disp;
struct wl_registry* reg;
struct xdg_surface* xrfc;
void createWindow(int W, int H, const char* title) {
	w = W;
	h = H;

	img_resize = img_resz_def;

	for (int i = 0; i < 348; i++) {is_key_down[i] = false;}

	disp = wl_display_connect(0);
	reg = wl_display_get_registry(disp);
	wl_registry_add_listener(reg, &reg_list, 0);
	wl_display_roundtrip(disp);

	srfc = wl_compositor_create_surface(comp);
	struct wl_callback* cb = wl_surface_frame(srfc);
	wl_callback_add_listener(cb, &cb_list, 0);

	xrfc = xdg_wm_base_get_xdg_surface(sh, srfc);
	xdg_surface_add_listener(xrfc, &xrfc_list, 0);
	top = xdg_surface_get_toplevel(xrfc);
	xdg_toplevel_add_listener(top, &top_list, 0);
	if (deco_mgr) {
		deco = zxdg_decoration_manager_v1_get_toplevel_decoration(deco_mgr, top);
		zxdg_toplevel_decoration_v1_set_mode(deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	else {
		fprintf(stderr, "WARNING: Wayland Server Does Not Support SSD\n");
		fprintf(stdout, "Falling Back to CSD\n");
		//setup CSD
		SSD = false;
		resz();
		minimise_button = decodeQOI("minimise.qoi");
		close_button = decodeQOI("close.qoi");
	}
	xdg_toplevel_set_title(top, title);
	
	wl_surface_commit(srfc);
}

bool windowShouldClose() {return (!wl_display_dispatch(disp) || cls);}

void destroyWindow() {
	if (kb) {
		wl_keyboard_destroy(kb);
	}
	wl_seat_release(seat);
	if (bfr) {
		wl_buffer_destroy(bfr);
	}
	xdg_toplevel_destroy(top);
	xdg_surface_destroy(xrfc);
	wl_surface_destroy(srfc);
	wl_display_disconnect(disp);
}

void setBuffer(uint8_t* new_img) {
	img = new_img;
}

void waitForFrame() {
	frame_pending = true;
	while (frame_pending) {
		if (wl_display_dispatch(disp) == -1) {
			break; // compositor closed
		}
	}
}

bool isKeyDown(KeyboardKey key) {
	return is_key_down[(int)key];
}

bool isKeyUp(KeyboardKey key) {
	return !is_key_down[(int)key];
}
