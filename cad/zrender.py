#!/usr/bin/env python3
"""zrender.py — small software renderer with a real z-buffer (no GL needed), anti-aliased, flat shading, 2 lights.
Used by render_scene.py and render_preview.py. Needs numpy, numba, scipy, pillow."""
import numpy as np
import numba
from scipy.ndimage import gaussian_filter
from PIL import Image, ImageDraw


@numba.njit(cache=True)
def _raster(tri, dep, col, W, H):
    img = np.zeros((H, W, 3), np.float32)
    zb = np.full((H, W), -1e18, np.float64)
    cov = np.zeros((H, W), np.float32)
    for i in range(tri.shape[0]):
        x0, y0 = tri[i, 0, 0], tri[i, 0, 1]
        x1, y1 = tri[i, 1, 0], tri[i, 1, 1]
        x2, y2 = tri[i, 2, 0], tri[i, 2, 1]
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-12:
            continue
        xmin = max(int(np.floor(min(x0, x1, x2))), 0); xmax = min(int(np.ceil(max(x0, x1, x2))), W - 1)
        ymin = max(int(np.floor(min(y0, y1, y2))), 0); ymax = min(int(np.ceil(max(y0, y1, y2))), H - 1)
        for y in range(ymin, ymax + 1):
            for x in range(xmin, xmax + 1):
                px, py = x + 0.5, y + 0.5
                a = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / den
                b = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / den
                c = 1.0 - a - b
                if a < -1e-6 or b < -1e-6 or c < -1e-6:
                    continue
                z = a * dep[i, 0] + b * dep[i, 1] + c * dep[i, 2]
                if z > zb[y, x]:
                    zb[y, x] = z
                    img[y, x, 0] = col[i, 0]; img[y, x, 1] = col[i, 1]; img[y, x, 2] = col[i, 2]
                    cov[y, x] = 1.0
    return img, cov


def camera(el, az):
    el, az = np.radians(el), np.radians(az)
    cam = np.array([np.cos(el) * np.sin(az), -np.cos(el) * np.cos(az), np.sin(el)])   # toward the camera, front = -Y
    u = np.cross([0, 0, 1], cam); u /= np.linalg.norm(u)
    if np.linalg.norm(u) < 1e-9: u = np.array([1.0, 0, 0])
    v = np.cross(cam, u)
    return cam, u, v


def render(parts, el=20, az=-30, width=1400, ssaa=3, background="blue", shadow=True, margin=0.07, out=None):
    """parts: dict name -> trimesh with face_colors. Returns an RGB uint8 array (and saves it when out is given)."""
    cam, u, v = camera(el, az)
    key = np.array([-0.6, -0.5, 0.65]); key /= np.linalg.norm(key)
    rim = np.array([0.7, 0.6, 0.3]); rim /= np.linalg.norm(rim)
    T, C, D = [], [], []
    for name, m in parts.items():
        fc = np.array(m.visual.face_colors[0][:3]) / 255.0
        t, n = m.triangles, m.face_normals
        keep = (n @ cam) > 0.0
        t, n = t[keep], n[keep]
        kd = np.clip(n @ key, 0, 1); rd = np.clip(n @ rim, 0, 1)
        h = (key + cam); h /= np.linalg.norm(h)
        spec = np.clip(n @ h, 0, 1) ** 28 * (0.12 if "table" in name else 0.35)
        c = (fc[None] * (0.30 + 0.80 * kd)[:, None] * np.array([1.04, 0.99, 0.92])
             + (0.12 * rd)[:, None] * np.array([0.55, 0.65, 0.9]) + spec[:, None])
        T.append(np.stack([t @ u, t @ v], axis=2)); D.append(t @ cam); C.append(np.clip(c, 0, 1))
    T = np.concatenate(T); D = np.concatenate(D); C = np.concatenate(C).astype(np.float32)
    xs, ys = T[..., 0], T[..., 1]
    xmin, xmax, ymin, ymax = xs.min(), xs.max(), ys.min(), ys.max()
    pad = margin * max(xmax - xmin, ymax - ymin)
    xmin, xmax, ymin, ymax = xmin - pad, xmax + pad, ymin - pad * 1.2, ymax + pad
    W = int(width * ssaa); s = W / (xmax - xmin); H = int(np.ceil((ymax - ymin) * s))
    P = np.empty_like(T)
    P[..., 0] = (T[..., 0] - xmin) * s; P[..., 1] = (ymax - T[..., 1]) * s
    img, cov = _raster(P.astype(np.float64), D.astype(np.float64), C, W, H)
    # downsample (anti-alias)
    w2, h2 = W // ssaa, H // ssaa
    img = img[:h2 * ssaa, :w2 * ssaa].reshape(h2, ssaa, w2, ssaa, 3).mean(axis=(1, 3))
    cov = cov[:h2 * ssaa, :w2 * ssaa].reshape(h2, ssaa, w2, ssaa).mean(axis=(1, 3))
    # un-premultiply colour at the edges
    img = np.where(cov[..., None] > 1e-3, img / np.maximum(cov[..., None], 1e-3), 0)
    # background
    yy = np.linspace(0, 1, h2)[:, None, None]
    if background == "blue":
        top, bot = np.array([0.90, 0.95, 1.0]), np.array([0.72, 0.85, 0.96])
    else:
        top, bot = np.array([0.70, 0.60, 0.52]), np.array([0.34, 0.28, 0.24])
    bg = top[None, None] * (1 - yy) + bot[None, None] * yy
    bg = np.broadcast_to(bg, (h2, w2, 3)).copy()
    if shadow:                                           # soft contact shadow of the housing footprint, offset by the key light
        sm = Image.new("L", (w2, h2), 0); d = ImageDraw.Draw(sm)
        fx, fy = 175.0, 175.0
        pts = []
        for cx_, cy_ in ((-fx, -fy), (fx, -fy), (fx, fy), (-fx, fy)):
            q = np.array([cx_ + 35, cy_ + 25, -10.0])
            pts.append(((q @ u - xmin) * s / ssaa, (ymax - q @ v) * s / ssaa))
        d.polygon(pts, fill=255)
        sm = np.asarray(sm, np.float32) / 255.0
        sm = gaussian_filter(sm, sigma=0.012 * w2)
        bg = bg * (1 - 0.30 * sm[..., None]) + np.array([0.15, 0.25, 0.4])[None, None] * 0.30 * sm[..., None] * (background == "blue")
    out_img = img * cov[..., None] + bg * (1 - cov[..., None])
    arr = (np.clip(out_img, 0, 1) * 255).astype(np.uint8)
    if out:
        Image.fromarray(arr).save(out)
    return arr
