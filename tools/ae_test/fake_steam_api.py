#!/usr/bin/env python3
"""A stand-in for Steam's public web API (ISteamRemoteStorage GetCollectionDetails / GetPublishedFileDetails) on this
namespace's loopback, for the harness's "helper" (a game runs in a network namespace of its own: a server of the test
process is out of its reach). Usage: fake_steam_api.py <port> <collection id> <title> <item id> [<item id> ...]
Every question is printed (the harness keeps it in helper.log)."""
import http.server
import json
import sys


def main():
    port, collection, title, items = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4:]

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
            print(f"asked {self.path} {body}", flush=True)
            if "GetCollectionDetails" in self.path:
                answer = {"response": {"result": 1, "resultcount": 1, "collectiondetails": [
                    {"publishedfileid": collection, "result": 1,
                     "children": [{"publishedfileid": item, "sortorder": n, "filetype": 0}
                                  for n, item in enumerate(items, 1)]}]}}
            else:
                answer = {"response": {"result": 1, "resultcount": 1, "publishedfiledetails": [
                    {"publishedfileid": collection, "result": 1, "title": title}]}}
            data = json.dumps(answer).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *arguments):
            pass

    http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
