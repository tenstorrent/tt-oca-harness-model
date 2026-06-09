-- pandoc-grid-tables.lua
--
-- Render every Markdown table as a "house style" longtable to match the
-- SystemC Virtual Platform document set:
--   * light-blue filled header row (\rowcolor{tblhead})
--   * full vertical column rules + outer border (|p|p|...|)
--   * booktabs-free horizontal rules: top, under-header (repeated on page
--     breaks via \endhead), and bottom only (no inter-row lines)
--   * left-aligned, wrapping p-columns sized from the widest cell per column
--
-- Requires the preamble to define color `tblhead` and load `colortbl`
-- (see the document header-includes / build_customer_guide.sh).

local stringify = pandoc.utils.stringify
local blocks_to_inlines = pandoc.utils.blocks_to_inlines

-- Render a table cell's blocks to a single line of LaTeX (no paragraph break).
local function cell_to_latex(cell)
  local inlines = blocks_to_inlines(cell.contents)
  local s = pandoc.write(pandoc.Pandoc({ pandoc.Plain(inlines) }), 'latex')
  return (s:gsub("%s+$", ""))
end

-- Plain-text length of a cell (used to size columns).
local function cell_len(cell)
  return #stringify(cell.contents)
end

local function collect_body_rows(tbl)
  local rows = {}
  for _, b in ipairs(tbl.bodies) do
    for _, r in ipairs(b.body) do
      rows[#rows + 1] = r
    end
  end
  return rows
end

function Table(tbl)
  local ncol = #tbl.colspecs
  if ncol == 0 then return nil end

  local head_rows = tbl.head.rows
  local body_rows = collect_body_rows(tbl)

  -- Column widths: proportional to the widest cell (header or body) per column.
  local maxlen = {}
  for i = 1, ncol do maxlen[i] = 1 end
  local function scan(rows)
    for _, row in ipairs(rows) do
      for i, cell in ipairs(row.cells) do
        local l = cell_len(cell)
        if l > maxlen[i] then maxlen[i] = l end
      end
    end
  end
  scan(head_rows)
  scan(body_rows)

  local total = 0
  for i = 1, ncol do total = total + maxlen[i] end

  local USABLE = 0.94 -- fraction of \linewidth available after inter-column rules
  local spec = "|"
  for i = 1, ncol do
    local w = (maxlen[i] / total) * USABLE
    if w < 0.06 then w = 0.06 end
    spec = spec .. string.format(">{\\raggedright\\arraybackslash}p{%.3f\\linewidth}|", w)
  end

  local out = {}
  out[#out + 1] = "\\begin{longtable}{" .. spec .. "}"
  out[#out + 1] = "\\hline"
  for _, row in ipairs(head_rows) do
    local cells = {}
    for _, cell in ipairs(row.cells) do cells[#cells + 1] = cell_to_latex(cell) end
    out[#out + 1] = "\\rowcolor{tblhead}" .. table.concat(cells, " & ") .. " \\\\"
  end
  out[#out + 1] = "\\hline"
  out[#out + 1] = "\\endhead"
  for _, row in ipairs(body_rows) do
    local cells = {}
    for _, cell in ipairs(row.cells) do cells[#cells + 1] = cell_to_latex(cell) end
    -- \hline after every row -> full grid (column + row separator lines)
    out[#out + 1] = table.concat(cells, " & ") .. " \\\\ \\hline"
  end
  out[#out + 1] = "\\end{longtable}"

  return pandoc.RawBlock('latex', table.concat(out, "\n"))
end
