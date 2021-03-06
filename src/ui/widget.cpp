#include "widget.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>

namespace gobang::ui {

Widget::Widget()
	: UiObject(__class__()), m_setup(false) {

	m_width = m_height = CW_USEDEFAULT;
	m_posx = m_posy = CW_USEDEFAULT;
	m_title = NULL;
	m_title_len = 0;

	set_min_bound();
	set_max_bound();

	m_preclicked = false;

	m_enable_track = false;
	m_on_tracking  = false;
}

Widget::Widget(int width, int height)
	: UiObject(__class__()), m_setup(false) {

	m_posx = m_posy = CW_USEDEFAULT;
	m_title = NULL;
	m_title_len = 0;

	m_preclicked = false;

	set_min_bound();
	set_max_bound();

	m_enable_track = false;
	m_on_tracking  = false;

	resize(width, height);
}

void Widget::resize(int width, int height) {

	m_width = width;
	m_height = height;
	if (!m_setup) return;

	SetWindowPos(
		m_hwnd, NULL,
		0, 0,
		m_width, m_height,
		SWP_NOMOVE | SWP_NOZORDER);
}

void Widget::move(int x, int y) {
	m_posx = x;
	m_posy = y;
	if (!m_setup) return;

	SetWindowPos(
		m_hwnd, NULL,
		m_posx, m_posy,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER);
}

void Widget::set_title(const char *title) {
	size_t len = strlen(title);
	if (m_title_len >= len) {
		strcpy(m_title, title);
	} else {
		if (m_title) free(m_title);
		m_title = strdup(title);
		m_title_len = len;
	}
	if (m_setup) {
		SetWindowText(m_hwnd, m_title);
	}
}

void Widget::enable_track(bool enable) {
	m_enable_track = enable;
	if (!m_enable_track) {
		m_on_tracking = false;
	}
}

void Widget::redraw(bool erase) {
	if (!m_setup) return;
	RECT rect;
	GetClientRect(m_hwnd, &rect);
	InvalidateRect(m_hwnd, &rect, erase);
}

void Widget::set_min_bound(int width, int height) {
	m_min_width  = width;
	m_min_height = height;
}

void Widget::set_max_bound(int width, int height) {
	m_max_width  = width;
	m_max_height = height;
}

void Widget::setup(const Widget *parent) {
	if (m_setup) {
		if (m_hwnd && parent->m_hwnd) {
			SetParent(m_hwnd, parent->m_hwnd);
		}
		return;
	}

	WNDCLASSEX wc = { };
	wc.lpfnWndProc   = Widget::dispatch;
	wc.lpszClassName = get_class();
	wc.style         = CS_DBLCLKS;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.cbSize        = sizeof(WNDCLASSEX);

	ATOM result = RegisterClassEx(&wc);

	HWND hparent = parent ? parent->m_hwnd : NULL;
	DWORD style  = parent ? WS_CHILD | WS_VISIBLE :
		WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;

	m_hwnd = CreateWindowEx(
		0,
		get_class(),
		m_title,
		style,
		m_posx, m_posy,
		m_width, m_height,
		hparent,
		NULL,
		NULL,
		this
	);

	assert(m_hwnd != NULL);
	if (m_hwnd == NULL) abort();

	m_setup = true;
}

void Widget::show(int type) {
	if (m_setup) {
		ShowWindow(m_hwnd, type);
	}
}

bool Widget::add_widget(Widget *widget) {
	if (!m_setup) setup();
	widget->setup(this);
	return true;
}

LRESULT CALLBACK Widget::dispatch(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	Widget *widget = reinterpret_cast<Widget*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg) {
		case WM_CREATE: {
			CREATESTRUCT *pcs = reinterpret_cast<CREATESTRUCT*>(lparam);
			widget = reinterpret_cast<Widget*>(pcs->lpCreateParams);
			widget->m_hwnd = hwnd;
			SetWindowLongPtr(widget->m_hwnd, GWLP_USERDATA, (LONG_PTR)widget);
			if (GetParent(widget->m_hwnd) == NULL) {
				RECT window, client;
				GetWindowRect(widget->m_hwnd, &window);
				GetClientRect(widget->m_hwnd, &client);
				window.right  -= window.left;
				window.bottom -= window.top;
				client.right  -= client.left;
				client.bottom -= client.top;
				SetWindowPos(widget->m_hwnd, NULL, 0, 0,
					window.right - client.right + widget->m_width,
					window.bottom - client.bottom + widget->m_height,
					SWP_NOMOVE | SWP_NOZORDER);
			}
			widget->m_setup = true;

			widget->created();

			printf("@%s [%p] created.\n", widget->get_class(), widget->m_hwnd);
		}
		break;
		case WM_SETFOCUS: {
			SetFocus(hwnd);
		}
		break;
		case WM_WINDOWPOSCHANGED: {
			WINDOWPOS *winpos = reinterpret_cast<WINDOWPOS*>(lparam);
  			if (!(winpos->flags & SWP_NOSIZE)) {
  				RECT window, client;
  				GetWindowRect(hwnd, &window);
  				GetClientRect(hwnd, &client);
  				widget->m_width  = winpos->cx - (window.right - window.left - client.right);
  				widget->m_height = winpos->cy - (window.bottom - window.top - client.bottom);
  			}
  			if (!(winpos->flags & SWP_NOMOVE)) {
  				widget->m_posx = winpos->x;
  				widget->m_posy = winpos->y;
  			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		break;
		case WM_SYSCOMMAND: {
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		break;
		case WM_GETMINMAXINFO: {
			if (widget == nullptr) break;
			MINMAXINFO *info = reinterpret_cast<MINMAXINFO*>(lparam);
			int ncwidth = 0, ncheight = 0;
			{
				RECT window, client;
				GetWindowRect(widget->m_hwnd, &window);
				GetClientRect(widget->m_hwnd, &client);
				ncwidth  = window.right - window.left - client.right;
				ncheight = window.bottom - window.top - client.bottom;
			}
			auto [minw, minh] = widget->get_min_bound();
			auto [maxw, maxh] = widget->get_max_bound();
			if (minw != -1) info->ptMinTrackSize.x = minw + ncwidth;
			if (minh != -1) info->ptMinTrackSize.y = minh + ncheight;
			if (maxw != -1) info->ptMaxTrackSize.x = maxw + ncwidth;
			if (maxh != -1) info->ptMaxTrackSize.y = maxh + ncheight;
		}
		break;
		case WM_MOVE: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->moved(posx, posy);
		}
		break;
		case WM_SIZE: {
			int width  = LOWORD(lparam);
			int height = HIWORD(lparam);
			widget->resized(width, height, wparam);
		}
		break;
		case WM_MOUSEHOVER: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->mouse_hover(posx, posy);
		}
		break;
		case WM_MOUSELEAVE: {
			widget->mouse_leave();
			widget->m_preclicked  = false;
			widget->m_on_tracking = false;
		}
		break;
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->mouse_press(posx, posy, wparam);

			widget->m_preclicked = true;
			widget->m_clicked_posx = posx;
			widget->m_clicked_posy = posy;
		}
		break;
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->mouse_release(posx, posy, wparam);

			if (widget->m_preclicked
				&& posx == widget->m_clicked_posx
				&& posy == widget->m_clicked_posy) {
				widget->clicked(posx, posy, wparam);
			}
			widget->m_preclicked = false;
		}
		break;
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->double_clicked(posx, posy, wparam);
		}
		break;
		case WM_KEYDOWN: {
			widget->Widget::key_down(wparam);
		}
		break;
		case WM_MOUSEMOVE: {
			int posx = GET_X_LPARAM(lparam);
			int posy = GET_Y_LPARAM(lparam);
			widget->mouse_move(posx, posy, wparam);

			if (widget->m_enable_track && !widget->m_on_tracking) {
				TRACKMOUSEEVENT track = { };
				track.hwndTrack   = widget->m_hwnd;
				track.dwFlags     = TME_HOVER | TME_LEAVE;
				track.dwHoverTime = 1;
				track.cbSize      = sizeof(TRACKMOUSEEVENT);
				TrackMouseEvent(&track);
				widget->m_on_tracking = true;
			}
		}
		break;
		case WM_MOUSEWHEEL: {
			RECT window;
			GetWindowRect(hwnd, &window);
			int posx  = GET_X_LPARAM(lparam) - window.left;
			int posy  = GET_Y_LPARAM(lparam) - window.top;
			int delta = GET_WHEEL_DELTA_WPARAM(wparam);
			widget->wheel_change(posx, posy, delta, LOWORD(wparam));
		}
		break;
		case WM_PAINT: {
			widget->render();
		}
		break;
		case WM_CLOSE: {
			DestroyWindow(hwnd);
		}
    	break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		}
		break;
		default: {
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
	}

	return 0;
}

int Widget::width() const {
	return m_width == CW_USEDEFAULT ? 0 : m_width;
}

int Widget::height() const {
	return m_height == CW_USEDEFAULT ? 0 : m_height;
}

int Widget::x() const {
	return m_posx == CW_USEDEFAULT ? 0 : m_posx;
}

int Widget::y() const {
	return m_posy == CW_USEDEFAULT ? 0 : m_posy;
}

std::tuple<int, int> Widget::get_min_bound() const {
	return std::make_tuple(m_min_width, m_min_height);
}

std::tuple<int, int> Widget::get_max_bound() const {
	return std::make_tuple(m_max_width, m_max_height);
}

Widget* Widget::parent() {
	HWND hparent = GetParent(m_hwnd);
	if (!hparent || hparent == m_hwnd ||
		hparent == GetWindow(m_hwnd, GW_OWNER)) {
		return nullptr;
	}
	return reinterpret_cast<Widget*>(GetWindowLongPtr(hparent, GWLP_USERDATA));
}

void Widget::mouse_hover(int x, int y) {

}

void Widget::mouse_leave() {

}

void Widget::mouse_press(int x, int y, int key_state) {

}

void Widget::mouse_release(int x, int y, int key_state) {

}

void Widget::mouse_move(int x, int y, int key_state) {

}

void Widget::wheel_change(int x, int y, int delta, int key_state) {
	
}

void Widget::render() {
	PAINTSTRUCT ps;
	BeginPaint(m_hwnd, &ps);

	EndPaint(m_hwnd, &ps);
}

};
