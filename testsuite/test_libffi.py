import subprocess
import pathlib

TEST_PATH = pathlib.Path(__file__).parents[0].resolve()

class BaseTestClass:
    def test_libffi(self, selenium, libffi_test):
        res = selenium.run_js(
            """
            window.TestModule = await Module();
            """
        )
        selenium.run_js(
            f"""
            try {{
                TestModule._test__{libffi_test}();
            }} catch(e){{
                if(e.name !== "ExitStatus"){{
                    throw e;
                }}
                if(e.status !== 0){{
                    throw new Error(`Terminated with nonzero status code ${{e.status}}: ` + e.message);
                }}
            }}
            """
        )

class TestCall(BaseTestClass):
    TEST_BUILD_DIR = "libffi.call.test"

class TestClosures(BaseTestClass):
    TEST_BUILD_DIR = "libffi.closures.test"


def pytest_generate_tests(metafunc):
    test_build_dir = metafunc.cls.TEST_BUILD_DIR
    test_names = [x.stem for x in (TEST_PATH / test_build_dir).glob("*.o")]
    metafunc.parametrize("libffi_test", test_names)

if __name__ == "__main__":
    subprocess.call(["emscripten-test-stuff/build-tests.sh", "libffi.call"])
