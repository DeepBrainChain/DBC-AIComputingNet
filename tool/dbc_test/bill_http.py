import time
import BaseHTTPServer
import urllib
import json
import base64
import json

HOST_NAME = '10.10.254.198'
PORT_NUMBER = 8080
CONIFRM_PATH = '/auth_task'

class HttpHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    def _set_headers(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()

    def _json_encode(self, data):
        json_data = json.loads(data)
        return json_data

    def _get_handler(self, data):
        json_data = self._json_encode(data)

    def _post_handler(self, data):
        retVal = {}
        json_data = self._json_encode(data)

        retVal["status"] = 0
        retVal["contract_state"] = "Active"
        retVal["report_cycle"] = 1
        return json.dumps(retVal)

    def do_HEAD(self):
        self._set_headers()

    def do_GET(self):
        self._set_headers()
        #get request params
        path = self.path
        query = urllib.splitquery(path)
        self._get_handler(query[1]);

    def do_POST(self):
        self._set_headers()
        #get post data
        post_data = self.rfile.read(int(self.headers['content-length']))
        post_data = urllib.unquote(post_data).decode("utf-8", 'ignore')
        retStr = self._post_handler(post_data)
        self.wfile.write(retStr)

if __name__ == '__main__':
    server_class = BaseHTTPServer.HTTPServer
    httpd = server_class((HOST_NAME, PORT_NUMBER), HttpHandler)
    print time.asctime(), "Server Starts - %s:%s" % (HOST_NAME, PORT_NUMBER)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    print time.asctime(), "Server Stops - %s:%s" % (HOST_NAME, PORT_NUMBER)