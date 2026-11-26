"""Ed25519 (RFC 8032) in plain Python: public key, sign and verify. Slow (milliseconds a call) and not constant time: for the party
service's small traffic and its tests only; the private half it signs with is the service's own key, read from the key file the service is started with."""
import hashlib

P = 2 ** 255 - 19
Q = 2 ** 252 + 27742317777372353535851937790883648493
D = -121665 * pow(121666, P - 2, P) % P
I = pow(2, (P - 1) // 4, P)


def _inv(x):
    return pow(x, P - 2, P)


def _recover_x(y, sign):
    if y >= P:
        return None
    x2 = (y * y - 1) * _inv(D * y * y + 1) % P
    if x2 == 0:
        return None if sign else 0
    x = pow(x2, (P + 3) // 8, P)
    if (x * x - x2) % P:
        x = x * I % P
    if (x * x - x2) % P:
        return None
    if (x & 1) != sign:
        x = P - x
    return x


_GY = 4 * _inv(5) % P
_GX = _recover_x(_GY, 0)
G = (_GX, _GY, 1, _GX * _GY % P)


def _add(a, b):
    a1 = (a[1] - a[0]) * (b[1] - b[0]) % P
    b1 = (a[1] + a[0]) * (b[1] + b[0]) % P
    c1 = 2 * a[3] * b[3] * D % P
    d1 = 2 * a[2] * b[2] % P
    e, f, g, h = b1 - a1, d1 - c1, d1 + c1, b1 + a1
    return (e * f % P, g * h % P, f * g % P, e * h % P)


def _mul(s, point):
    result = (0, 1, 1, 0)
    while s > 0:
        if s & 1:
            result = _add(result, point)
        point = _add(point, point)
        s >>= 1
    return result


def _equal(a, b):
    return (a[0] * b[2] - b[0] * a[2]) % P == 0 and (a[1] * b[2] - b[1] * a[2]) % P == 0


def _compress(point):
    zi = _inv(point[2])
    x, y = point[0] * zi % P, point[1] * zi % P
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")


def _decompress(data):
    if len(data) != 32:
        return None
    y = int.from_bytes(data, "little")
    sign = y >> 255
    y &= (1 << 255) - 1
    x = _recover_x(y, sign)
    return None if x is None else (x, y, 1, x * y % P)


def _h(data):
    return int.from_bytes(hashlib.sha512(data).digest(), "little")


def _secret_expand(seed):
    if len(seed) != 32:
        raise ValueError("seed must be 32 bytes")
    h = hashlib.sha512(seed).digest()
    a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    return a, h[32:]


def public_key(seed):
    a, _ = _secret_expand(seed)
    return _compress(_mul(a, G))


def sign(seed, message):
    a, prefix = _secret_expand(seed)
    pub = _compress(_mul(a, G))
    r = _h(prefix + message) % Q
    big_r = _compress(_mul(r, G))
    k = _h(big_r + pub + message) % Q
    s = (r + k * a) % Q
    return big_r + int.to_bytes(s, 32, "little")


def _small_order(point):
    """A point of order 1, 2, 4 or 8 (the eight encodings that make any message verify): never a real key or commitment."""
    return _equal(_mul(8, point), (0, 1, 1, 0))


def verify(pub, message, signature):
    if len(pub) != 32 or len(signature) != 64:
        return False
    a = _decompress(pub)
    if a is None or _small_order(a):
        return False
    r_point = _decompress(signature[:32])
    if r_point is None or _small_order(r_point):
        return False
    s = int.from_bytes(signature[32:], "little")
    if s >= Q:
        return False
    k = _h(signature[:32] + pub + message) % Q
    return _equal(_mul(s, G), _add(r_point, _mul(k, a)))
