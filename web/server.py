#!/usr/bin/env python3
"""Мост между платой и браузером: раздаёт страницу из web/ и пересылает команды в порт.

Нужен, когда плата подключена к одному компьютеру, а управлять хочется с другого
(или с телефона): Web Serial в браузере видит только свои USB-порты.

    python3 web/server.py --serial /dev/ttyACM0 --http 8000

Дальше открыть http://<адрес этого компьютера>:8000 в любом браузере.
Зависимостей нет (pyserial используется, если установлен, иначе termios - Linux/macOS).

    python3 web/server.py --https 8443

дополнительно поднимает HTTPS с самоподписанным сертификатом (создаётся через openssl в web/.cert/).
Нужно, чтобы со страницы можно было подключать и плату, воткнутую в сам компьютер с браузером:
Web Serial работает только на HTTPS или localhost. Плату на сервере можно не подключать: --serial none.

API:
    GET  /api/info   - {"serial": порт или null, "online": плата отвечает, "http": порт, "https": порт или null}
    GET  /api/events - поток Server-Sent Events: data: <hex байт от платы>, event: status (online/offline)
    POST /api/send   - тело: пакеты по 5 байт [A5 cmd id lo hi], уходят в плату как есть
"""
import argparse
import http.server
import json
import os
import queue
import select
import socket
import ssl
import subprocess
import sys
import threading
import time

PACKET_START = 0xA5
GET_ALL = bytes([PACKET_START, 0x03, 0xFF, 0, 0])
WEB_DIR = os.path.dirname(os.path.abspath(__file__))
CERT_DIR = os.path.join(WEB_DIR, '.cert')


# ---------------- порт ----------------
class SerialPort:
    """Минимальный порт: pyserial, если есть, иначе termios."""

    def __init__(self, path, baud):
        try:
            import serial
            self._ser = serial.Serial(path, baud, timeout=0.2)
            self.read = lambda: self._ser.read(256)
            self.write = self._ser.write
            self.close = self._ser.close
            return
        except ImportError:
            pass
        import termios
        speed = getattr(termios, f'B{baud}')
        self._fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
        attrs = termios.tcgetattr(self._fd)
        attrs[0] = 0                                                    # iflag
        attrs[1] = 0                                                    # oflag
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL         # cflag
        attrs[3] = 0                                                    # lflag
        attrs[4] = attrs[5] = speed
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self._fd, termios.TCSANOW, attrs)
        termios.tcflush(self._fd, termios.TCIOFLUSH)

    def read(self):
        ready, _, _ = select.select([self._fd], [], [], 0.2)
        if not ready:
            return b''
        data = os.read(self._fd, 256)
        if not data:                    # select сказал "готово", а данных нет - устройство пропало
            raise OSError('устройство отключено')
        return data

    def write(self, data):
        os.write(self._fd, data)

    def close(self):
        os.close(self._fd)


# ---------------- мост ----------------
class Bridge:
    def __init__(self, path, baud):
        self.path, self.baud = path, baud
        self.port = None
        self.online = False
        self.clients = set()
        self.lock = threading.Lock()
        self._last_error = None
        if path:
            threading.Thread(target=self._run, daemon=True).start()

    def _broadcast(self, message):
        with self.lock:
            for q in self.clients:
                q.put(message)

    def _set_online(self, online):
        if online != self.online:
            self.online = online
            log('плата ' + ('подключена' if online else 'недоступна'))
            self._broadcast(('status', 'online' if online else 'offline'))

    def _run(self):
        while True:
            try:
                self.port = SerialPort(self.path, self.baud)
                time.sleep(2.2)                     # плата перезагружается при открытии порта
                self._set_online(True)
                self._last_error = None
                self.port.write(GET_ALL)
                while True:
                    data = self.port.read()
                    if data:
                        self._broadcast(('data', data.hex()))
            except Exception as e:                  # нет устройства, отключили кабель, нет прав...
                if str(e) != self._last_error:      # одну и ту же ошибку не повторяем каждые 2 секунды
                    log(f'порт {self.path}: {e}')
                    self._last_error = str(e)
                self._set_online(False)
                if self.port:
                    try:
                        self.port.close()
                    except Exception:
                        pass
                    self.port = None
                time.sleep(2)

    def send(self, data):
        if not self.online or not self.port:
            return False
        self.port.write(data)
        return True

    def subscribe(self):
        q = queue.Queue()
        with self.lock:
            self.clients.add(q)
        return q

    def unsubscribe(self, q):
        with self.lock:
            self.clients.discard(q)


def log(text):
    print(time.strftime('%H:%M:%S'), text, flush=True)


# ---------------- HTTP ----------------
class Handler(http.server.SimpleHTTPRequestHandler):
    bridge = None
    ports = {}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def log_message(self, fmt, *args):         # статику и опросы не логируем
        pass

    def do_GET(self):
        if self.path.startswith('/api/events'):
            return self._events()
        if self.path.startswith('/api/info'):
            return self._info()
        return super().do_GET()

    def send_head(self):
        # GET и HEAD статики идут сюда. Раздаём только страницу: без скрытых файлов (.cert/key.pem!) и исходников
        rel = os.path.relpath(self.translate_path(self.path), WEB_DIR)
        if rel.split('?')[0] == 'cert.pem':     # публичный сертификат - чтобы добавить его в доверенные
            return self._send_cert()
        if rel.startswith('..') or any(part.startswith('.') for part in rel.split(os.sep) if part != '.') \
                or rel.endswith('.py'):
            self.send_error(404)
            return None
        return super().send_head()

    def _send_cert(self):
        path = os.path.join(CERT_DIR, 'cert.pem')
        if not os.path.exists(path):
            self.send_error(404)
            return None
        f = open(path, 'rb')
        self.send_response(200)
        self.send_header('Content-Type', 'application/x-x509-ca-cert')
        self.send_header('Content-Disposition', 'attachment; filename="colormusic.crt"')
        self.send_header('Content-Length', str(os.fstat(f.fileno()).st_size))
        self.end_headers()
        return f

    def do_POST(self):
        if not self.path.startswith('/api/send'):
            return self.send_error(404)
        length = int(self.headers.get('Content-Length') or 0)
        data = self.rfile.read(min(length, 256))
        valid = len(data) and len(data) % 5 == 0 and all(data[i] == PACKET_START for i in range(0, len(data), 5))
        if not valid:
            return self.send_error(400, 'expected 5-byte packets starting with A5')  # только латиница: идёт в строку статуса HTTP
        ok = self.bridge.send(data)
        self.send_response(204 if ok else 503)
        self.end_headers()

    def _info(self):
        body = json.dumps({'serial': self.bridge.path, 'online': self.bridge.online, **self.ports}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _events(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Cache-Control', 'no-cache')
        self.end_headers()
        q = self.bridge.subscribe()
        log(f'браузер подключился: {self.client_address[0]}')
        try:
            if not self.bridge.path:
                self._write_event('status', 'none')    # сервер без платы
            else:
                self._write_event('status', 'online' if self.bridge.online else 'offline')
            while True:
                try:
                    kind, payload = q.get(timeout=15)
                    self._write_event(kind, payload)
                except queue.Empty:
                    self.wfile.write(b': ping\n\n')     # держим соединение живым
                    self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.bridge.unsubscribe(q)
            log(f'браузер отключился: {self.client_address[0]}')

    def _write_event(self, kind, payload):
        prefix = '' if kind == 'data' else f'event: {kind}\n'
        self.wfile.write(f'{prefix}data: {payload}\n\n'.encode())
        self.wfile.flush()


class TLSServer(http.server.ThreadingHTTPServer):
    """HTTPS: рукопожатие в потоке запроса, чтобы медленный клиент не блокировал остальных."""

    def __init__(self, addr, handler, ctx):
        super().__init__(addr, handler)
        self.ctx = ctx

    def finish_request(self, request, client_address):
        request.settimeout(10)
        request = self.ctx.wrap_socket(request, server_side=True)
        request.settimeout(None)
        super().finish_request(request, client_address)

    def handle_error(self, request, client_address):
        if not isinstance(sys.exc_info()[1], (ssl.SSLError, OSError)):  # браузер отверг сертификат - это нормально
            super().handle_error(request, client_address)


def lan_addresses():
    """Адреса этого компьютера в локальной сети (для сертификата и подсказки)."""
    addrs = set()
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(('192.0.2.1', 9))          # пакеты не уходят, просто выбирается интерфейс по умолчанию
            addrs.add(s.getsockname()[0])
    except OSError:
        pass
    try:
        addrs.update(i[4][0] for i in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET))
    except OSError:
        pass
    addrs.discard('127.0.0.1')
    return sorted(addrs)


def ensure_cert(names):
    """Самоподписанный сертификат в web/.cert/, пересоздаётся при смене адресов."""
    cert, key, stamp = (os.path.join(CERT_DIR, f) for f in ('cert.pem', 'key.pem', 'names.txt'))
    want = ','.join(['DNS:localhost', 'IP:127.0.0.1'] + [f'IP:{a}' for a in names])
    if os.path.exists(cert) and os.path.exists(key) and open(stamp).read() == want:
        return cert, key
    os.makedirs(CERT_DIR, exist_ok=True)
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '3650',
                    '-subj', '/CN=colormusic', '-addext', f'subjectAltName={want}',
                    '-keyout', key, '-out', cert], check=True, capture_output=True)
    with open(stamp, 'w') as f:
        f.write(want)
    log('создан самоподписанный сертификат ' + cert)
    return cert, key


def main():
    ap = argparse.ArgumentParser(description='Мост плата <-> браузер для светомузыки')
    ap.add_argument('--serial', default='/dev/ttyACM0', help='порт платы (по умолчанию /dev/ttyACM0), none - без платы')
    ap.add_argument('--baud', type=int, default=9600)
    ap.add_argument('--http', type=int, default=8000, help='порт веб-сервера (по умолчанию 8000)')
    ap.add_argument('--https', type=int, default=0, help='порт HTTPS (для Web Serial с других компьютеров), 0 - не нужен')
    ap.add_argument('--bind', default='0.0.0.0', help='адрес (0.0.0.0 - доступно из сети)')
    args = ap.parse_args()

    serial_path = None if args.serial.lower() == 'none' else args.serial
    Handler.bridge = Bridge(serial_path, args.baud)
    Handler.ports = {'http': args.http, 'https': args.https or None}
    addrs = lan_addresses() if args.bind == '0.0.0.0' else [args.bind]

    servers = []
    http_srv = http.server.ThreadingHTTPServer((args.bind, args.http), Handler)
    servers.append(http_srv)
    log(f'плата: {serial_path or "не используется"}')
    log('страница: ' + '  '.join(f'http://{a}:{args.http}' for a in addrs or ['localhost']))
    if args.https:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(*ensure_cert(addrs))
        servers.append(TLSServer((args.bind, args.https), Handler, ctx))
        log('с Web Serial: ' + '  '.join(f'https://{a}:{args.https}' for a in addrs or ['localhost']))
    for srv in servers:
        srv.daemon_threads = True
    for srv in servers[1:]:
        threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        servers[0].serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    sys.exit(main())
