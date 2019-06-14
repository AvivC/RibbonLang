def test_math_expressions(execprint):
    assert execprint('1 + 2') == '3'
    assert execprint('10 + 20') == '30'
    assert execprint('1.5 + 2') == '3.5'
    assert execprint('1000 / 2') == '500'
    assert execprint('1000 / 2.0') == '500'
    assert execprint('-10 + 10') == '0'
    assert execprint('--10') == '10'
    assert execprint('20 * 0') == '0'
    assert execprint('20.0 * 1.5') == '30'


def test_variables(execute):
    assert execute('a = 10\nprint(a)') == '10'
