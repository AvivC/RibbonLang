import pytest


def test_math_expressions(execute):
    assert execute('1 + 2') == '3'
    assert execute('10 + 20') == '30'
    assert execute('1.5 + 2') == '3.5'
    assert execute('1000 / 2') == '500'
    assert execute('1000 / 2.0') == '500'
    assert execute('-10 + 10') == '0'
    assert execute('--10') == '10'
    assert execute('20 * 0') == '0'
    assert execute('20.0 * 1.5') == '30'


def test_variables(execute):
    assert execute('a = 10\na') == '10'
