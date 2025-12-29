wrk.method = "POST"
wrk.body   = string.rep("E", 400) .. "\n"  -- ~400B + newline
wrk.headers["Content-Type"] = "text/plain"

request = function()
    return wrk.format(nil, nil)
end