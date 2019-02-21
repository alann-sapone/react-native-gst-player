function isSame(a, b) {
    if (a.length != b.length) return false;
    if (a.filter(function (i) { return a.indexOf(i) < 0; }).length > 0)
        return false;
    if (b.filter(function (i) { return a.indexOf(i) < 0; }).length > 0)
        return false;
    return true;
};

function substract(a, b) {
    var r = {};

    for (var key in b) {
        if (Array.isArray(b[key])) {
            if (!a[key]) a[key] = [];
            if (!isSame(a[key], b[key]))
                r[key] = a[key];
        } else if (typeof (b[key]) == 'object') {
            if (!a[key]) a[key] = {};
            r[key] = substract(a[key], b[key]);
            if (Object.keys(r[key]).length == 0)
                delete r[key];
        } else {
            if (b[key] != a[key]) {
                r[key] = a[key];
            }
        }
    }
    return r;
}

export {
    substract
}