#pragma once

#include "stb.hpp"
#include <cstdint>
#include <vector>
#include <cmath>

static inline constexpr int32_t tex_scale(int32_t s)
{
	return s * stb::Img::size / 1000;
}

struct ivec2 {
	int32_t x;
	int32_t y;

	using own = ivec2;

	inline ivec2(int32_t x, int32_t y) :
		x(x),
		y(y)
	{
	}

	inline auto operator-(const own &other) const
	{
		return own(x - other.x, y - other.y);
	}

	inline auto operator+(const own &other) const
	{
		return own(x + other.x, y + other.y);
	}

	inline auto operator*(int32_t s) const
	{
		return own(x * s, y * s);
	}

	inline void operator-=(const own &other)
	{
		*this = *this - other;
	}

	inline void operator+=(const own &other)
	{
		*this = *this + other;
	}

	int32_t dot(const own &other) const
	{
		return x * other.x + y * other.y;
	}

	int32_t norm_tex(void) const
	{
		return std::sqrt(tex_scale(dot(*this)));
	}
};

inline int32_t min(int32_t a, int32_t b)
{
	return a < b ? a : b;
}

inline int32_t max(int32_t a, int32_t b)
{
	return a > b ? a : b;
}

struct Wall {
	ivec2 a;
	ivec2 b;
	int32_t ele_low;
	int32_t ele_up;
	int32_t w;
	int32_t h;
	
	Wall(ivec2 a, ivec2 b, int32_t ele_low, int32_t ele_up) :
		a(a),
		b(b),
		ele_low(ele_low),
		ele_up(ele_up),
		w((b - a).norm_tex()),
		h(tex_scale((ele_up - ele_low)))
	{
	}
};

class Renderer
{
	uint32_t *m_fb;
	uint32_t m_w;
	uint32_t m_h;
	int32_t m_wh;
	int32_t m_hh;
	int32_t m_wm;
	int32_t m_hm;

	std::vector<Wall> walls;

	stb::Img t0;

public:
	Renderer(uint32_t *fb, uint32_t w, uint32_t h) :
		m_fb(fb),
		m_w(w),
		m_h(h),
		m_wh(m_w / 2),
		m_hh(m_h / 2),
		m_wm(m_w - 1),
		m_hm(m_h - 1),
		t0("res/t0.png", false)
	{
		walls.emplace_back(Wall{
			ivec2(-500, 500),
			ivec2(2000, 3000),
			/*-800,
			200*/
			-500,
			500
		});
		walls.emplace_back(Wall{
			ivec2(-2000, 3000),
			ivec2(500, 500) + ivec2(-1200, 0),
			/*-800,
			200*/
			-500,
			500
		});
	}

	int32_t proj_x(const ivec2 &p)
	{
		return (p.x * m_hh) / p.y + m_wh;
	}

	int32_t proj_y(const ivec2 &p, int32_t ele)
	{
		return (ele * m_hh) / p.y + m_hh;
	}

	int32_t lerp(int32_t a, int32_t b, int32_t scale, int32_t x)
	{
		return ((scale - x) * a + x * b) / scale;
	}

	int32_t lerp_persp(int32_t a, int32_t b, int32_t za, int32_t zb, int32_t scale, int32_t x)
	{
		return ((scale - x) * a * zb + x * b * za) / ((scale - x) * zb + x * za);
	}

	int32_t lerp_z(int32_t za, int32_t zb, int32_t scale, int32_t x)
	{
		return scale * za * zb / ((scale - x) * zb + x * za);
	}

	void render(ivec2 camp, int32_t camele)
	{
		std::memset(m_fb, 0, m_w * m_h * sizeof(uint32_t));
		for (auto w : walls) {
			w.a -= camp;
			w.b -= camp;
			w.ele_low -= camele;
			w.ele_up -= camele;

			if (w.a.y <= 0 || w.b.y <= 0)
				continue;

			int32_t l = proj_x(w.a);
			int32_t lu = 0;
			int32_t r = proj_x(w.b);
			int32_t ru = w.w;

			if (l < 0) {
				lu = lerp_persp(w.w, 0, w.b.y, w.a.y, r - l, r);
				w.a.y = lerp_z(w.b.y, w.a.y, r - l, r);
				l = 0;
			}
			if (r > m_wm) {
				ru = lerp_persp(0, w.w, w.a.y, w.b.y, r - l, m_w - l);
				w.b.y = lerp_z(w.a.y, w.b.y, r - l, m_w - l);
				r = m_wm;
			}
			if (l > m_wm || r < 0 || l >= r)
				continue;

			int32_t ta = proj_y(w.a, w.ele_low);
			int32_t ba = proj_y(w.a, w.ele_up);
			int32_t tb = proj_y(w.b, w.ele_low);
			int32_t bb = proj_y(w.b, w.ele_up);

			int32_t rl = r - l;

			int32_t hh = lerp(0, w.h, w.ele_up - w.ele_low, -w.ele_low);

			for (int32_t i = l; i < r; i++) {
				auto col = m_fb + i * m_h;
				auto x = i - l;
				int32_t t = lerp(ta, tb, rl, x);
				int32_t tu = 0;
				if (t < 0) {
					tu = lerp(hh, tu, m_hh - t, m_hh);
					t = 0;
				}
				int32_t b = lerp(ba, bb, rl, x);
				int32_t bu = w.h;
				if (b > m_hm) {
					bu = lerp(hh, bu, b - m_hh, m_hh);
					b = m_hm;
				}
				int32_t bt = b - t;
				for (int32_t j = t; j < b; j++)
					col[j] = t0.sample(
						lerp_persp(lu, ru, w.a.y, w.b.y, rl, x),
						lerp(tu, bu, bt, j - t)
					);
			}
		}
	}
};