"""
Various common utilities for testing.
"""

import contextlib
import multiprocessing
import textwrap
import tempfile
import time
import os
import pathlib
import queue
import sys
import shutil

import pytest

ROOT_PATH = pathlib.Path(__file__).parents[0].resolve()
TEST_PATH = ROOT_PATH / "testsuite"


@contextlib.contextmanager
def set_webdriver_script_timeout(selenium, script_timeout):
    """Set selenium script timeout

    Parameters
    ----------
    selenum : SeleniumWrapper
       a SeleniumWrapper wrapper instance
    script_timeout : int | float
       value of the timeout in seconds
    """
    if script_timeout is not None:
        selenium.driver.set_script_timeout(script_timeout)
    yield
    # revert to the initial value
    if script_timeout is not None:
        selenium.driver.set_script_timeout(selenium.script_timeout)


def parse_driver_timeout(request):
    """Parse driver timeout value from pytest request object"""
    mark = request.node.get_closest_marker("driver_timeout")
    if mark is None:
        return None
    else:
        return mark.args[0]

def pytest_addoption(parser):
    group = parser.getgroup("general")
    group.addoption(
        "--test-path",
        action="store",
        default=TEST_PATH,
        help="Path to the build directory",
    )


class JavascriptException(Exception):
    def __init__(self, msg, stack):
        self.msg = msg
        self.stack = stack
        # In chrome the stack contains the message
        if self.stack and self.stack.startswith(self.msg):
            self.msg = ""

    def __str__(self):
        return "\n\n".join(x for x in [self.msg, self.stack] if x)


class SeleniumWrapper:
    JavascriptException = JavascriptException

    def __init__(
        self,
        *,
        server_port,
        test_path,
        test_dir,
        server_hostname="127.0.0.1",
        server_log=None,
        script_timeout=20,
    ):
        if test_path is None:
            test_path = TEST_PATH

        self.driver = self.get_driver()
        self.server_port = server_port
        self.server_hostname = server_hostname
        self.server_log = server_log
        
        test_path = pathlib.Path(test_path) / test_dir / "test.html"
        if not test_path.exists():
            # selenium does not expose HTTP response codes
            raise ValueError(
                f"{test_path.resolve()} does not exist!"
            )
        self.driver.get(f"http://{server_hostname}:{server_port}/{test_dir}/test.html")
        print(self.run_js("globalThis.TestModule = await globalThis.Module(); return typeof TestModule;"));
        self.javascript_setup()
        self.script_timeout = script_timeout
        self.driver.set_script_timeout(script_timeout)

    def javascript_setup(self):
        self.run_js("Error.stackTraceLimit = Infinity;")
        self.run_js(
            """
            window.sleep = function(ms){
                return new Promise(resolve => setTimeout(resolve, ms));
            };
            window.assert = function(cb, message=""){
                if(message !== ""){
                    message = "\\n" + message;
                }
                if(cb() !== true){
                    throw new Error(`Assertion failed: ${cb.toString().slice(6)}${message}`);
                }
            };
            window.assertAsync = async function(cb, message=""){
                if(message !== ""){
                    message = "\\n" + message;
                }
                if(await cb() !== true){
                    throw new Error(`Assertion failed: ${cb.toString().slice(12)}${message}`);
                }
            };
            function checkError(err, errname, pattern, pat_str, thiscallstr){
                if(typeof pattern === "string"){
                    pattern = new RegExp(pattern);
                }
                if(!err){
                    throw new Error(`${thiscallstr} failed, no error thrown`);
                }
                if(err.constructor.name !== errname){
                    throw new Error(
                        `${thiscallstr} failed, expected error ` +
                        `of type '${errname}' got type '${err.constructor.name}'`
                    );
                }
                if(!pattern.test(err.message)){
                    throw new Error(
                        `${thiscallstr} failed, expected error ` +
                        `message to match pattern ${pat_str} got:\n${err.message}`
                    );
                }
            }
            window.assertThrows = function(cb, errname, pattern){
                let pat_str = typeof pattern === "string" ? `"${pattern}"` : `${pattern}`;
                let thiscallstr = `assertThrows(${cb.toString()}, "${errname}", ${pat_str})`;
                let err = undefined;
                try {
                    cb();
                } catch(e) {
                    err = e;
                }
                checkError(err, errname, pattern, pat_str, thiscallstr);
            };
            window.assertThrowsAsync = async function(cb, errname, pattern){
                let pat_str = typeof pattern === "string" ? `"${pattern}"` : `${pattern}`;
                let thiscallstr = `assertThrowsAsync(${cb.toString()}, "${errname}", ${pat_str})`;
                let err = undefined;
                try {
                    await cb();
                } catch(e) {
                    err = e;
                }
                checkError(err, errname, pattern, pat_str, thiscallstr);
            };
            """,
        )

    @property
    def logs(self):
        logs = self.driver.execute_script("return window.logs;")
        if logs is not None:
            return "\n".join(str(x) for x in logs)
        else:
            return ""

    def clean_logs(self):
        self.driver.execute_script("window.logs = []")

    def run_js(self, code):
        """Run JavaScript code"""
        if isinstance(code, str) and code.startswith("\n"):
            # we have a multiline string, fix indentation
            code = textwrap.dedent(code)

        wrapper = """
            let cb = arguments[arguments.length - 1];
            let run = async () => { %s }
            (async () => {
                try {
                    let result = await run();
                    cb([0, result]);
                } catch (e) {
                    cb([1, e.message, e.stack, typeof e]);
                }
            })()
        """

        retval = self.driver.execute_async_script(wrapper % code)

        if retval[0] == 0:
            return retval[1]
        else:
            raise JavascriptException(retval[1], retval[2])

    @property
    def urls(self):
        for handle in self.driver.window_handles:
            self.driver.switch_to.window(handle)
            yield self.driver.current_url


class FirefoxWrapper(SeleniumWrapper):

    browser = "firefox"

    def get_driver(self):
        from selenium.webdriver import Firefox
        from selenium.webdriver.firefox.options import Options

        options = Options()
        options.add_argument("-headless")

        return Firefox(executable_path="geckodriver", options=options)


class ChromeWrapper(SeleniumWrapper):

    browser = "chrome"

    def get_driver(self):
        from selenium.webdriver import Chrome
        from selenium.webdriver.chrome.options import Options

        options = Options()
        options.add_argument("--headless")
        options.add_argument("--no-sandbox")
        options.add_argument("--js-flags=--expose-gc")
        return Chrome(options=options)


@pytest.fixture(params=["firefox", "chrome"], scope="class")
def selenium_class_scope(request, web_server_main):
    server_hostname, server_port, server_log = web_server_main
    if request.param == "firefox":
        cls = FirefoxWrapper
    elif request.param == "chrome":
        cls = ChromeWrapper
    else:
        assert False
    selenium = cls(
        test_path=request.config.option.test_path,
        test_dir=request.cls.TEST_BUILD_DIR,
        server_port=server_port,
        server_hostname=server_hostname,
        server_log=server_log,
    )
    with set_webdriver_script_timeout(
        selenium, script_timeout=parse_driver_timeout(request)
    ):
        try:
            yield selenium
        finally:
            print(selenium.logs)
            selenium.driver.quit()

@pytest.fixture(scope="function")
def selenium(selenium_class_scope, capsys):
    try:
        selenium_class_scope.clean_logs()
        yield selenium_class_scope
    finally:
        print(selenium_class_scope.logs)

@pytest.fixture(scope="session")
def web_server_main(request):
    """Web server that serves files in the build/ directory"""
    with spawn_web_server(request.config.option.test_path) as output:
        yield output


@pytest.fixture(scope="session")
def web_server_secondary(request):
    """Secondary web server that serves files build/ directory"""
    with spawn_web_server(request.config.option.test_path) as output:
        yield output


@pytest.fixture(scope="session")
def web_server_tst_data(request):
    """Web server that serves files in the src/tests/data/ directory"""
    with spawn_web_server(TEST_PATH / "data") as output:
        yield output


@contextlib.contextmanager
def spawn_web_server(test_dir=None):

    if test_dir is None:
        test_dir = TEST_PATH

    tmp_dir = tempfile.mkdtemp()
    log_path = pathlib.Path(tmp_dir) / "http-server.log"
    q = multiprocessing.Queue()
    p = multiprocessing.Process(target=run_web_server, args=(q, log_path, test_dir))

    try:
        p.start()
        port = q.get()
        hostname = "127.0.0.1"

        print(
            f"Spawning webserver at http://{hostname}:{port} "
            f"(see logs in {log_path})"
        )
        yield hostname, port, log_path
    finally:
        q.put("TERMINATE")
        p.join()
        shutil.rmtree(tmp_dir)


def run_web_server(q, log_filepath, test_dir):
    """Start the HTTP web server

    Parameters
    ----------
    q : Queue
      communication queue
    log_path : pathlib.Path
      path to the file where to store the logs
    """
    import http.server
    import socketserver

    os.chdir(test_dir)

    log_fh = log_filepath.open("w", buffering=1)
    sys.stdout = log_fh
    sys.stderr = log_fh

    class Handler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, format_, *args):
            print(
                "[%s] source: %s:%s - %s"
                % (self.log_date_time_string(), *self.client_address, format_ % args)
            )

        def end_headers(self):
            # Enable Cross-Origin Resource Sharing (CORS)
            self.send_header("Access-Control-Allow-Origin", "*")
            super().end_headers()

    with socketserver.TCPServer(("", 0), Handler) as httpd:
        host, port = httpd.server_address
        print(f"Starting webserver at http://{host}:{port}")
        httpd.server_name = "test-server"
        httpd.server_port = port
        q.put(port)

        def service_actions():
            try:
                if q.get(False) == "TERMINATE":
                    print("Stopping server...")
                    sys.exit(0)
            except queue.Empty:
                pass

        httpd.service_actions = service_actions
        httpd.serve_forever()


if (
    __name__ == "__main__"
    and multiprocessing.current_process().name == "MainProcess"
    and not hasattr(sys, "_pytest_session")
):
    with spawn_web_server():
        # run forever
        while True:
            time.sleep(1)
