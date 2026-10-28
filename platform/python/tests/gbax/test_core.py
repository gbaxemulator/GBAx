import pytest

def test_core_import():
    try:
        import gbax.core
    except:
        raise AssertionError
