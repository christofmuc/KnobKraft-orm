const state = { synths: [], query: '', status: 'all' };
const labels = { all: 'All', works: 'Works', beta: 'Beta', alpha: 'Alpha', in_progress: 'In progress' };
const body = document.getElementById('synthTableBody');
const count = document.getElementById('synthCountLabel');
const filters = document.getElementById('statusFilters');
function render() {
  const rows = state.synths.filter(row => (state.status === 'all' || row.status === state.status) && `${row.manufacturer} ${row.synth}`.toLowerCase().includes(state.query));
  body.replaceChildren();
  for (const row of rows) {
    const tr = document.createElement('tr');
    for (const value of [row.manufacturer, row.synth, labels[row.status]]) {
      const td = document.createElement('td'); td.textContent = value; tr.append(td);
    }
    tr.lastElementChild.className = `status ${row.status}`;
    tr.children[1].dataset.manufacturer = row.manufacturer;
    if (row.availability !== '2.9.0') {
      const note = document.createElement('small'); note.textContent = row.availability; tr.children[1].append(note);
    }
    body.append(tr);
  }
  if (!rows.length) {
    const td = document.createElement('td'); td.colSpan = 3; td.textContent = 'No matching synths. Try another name or choose All.';
    const tr = document.createElement('tr'); tr.append(td); body.append(tr);
  }
  count.textContent = `${rows.length} of ${state.synths.length} listed synths / families`;
  filters.querySelectorAll('button').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.status === state.status)));
}
document.getElementById('synthSearch').addEventListener('input', event => { state.query = event.target.value.trim().toLowerCase(); render(); });
async function load() {
  try {
    const response = await fetch('./data/supported-synths.json');
    if (!response.ok) throw new Error(`Compatibility data: ${response.status}`);
    const data = await response.json();
    if (!Array.isArray(data.synths) || !data.synths.length) throw new Error('Empty compatibility list');
    state.synths = data.synths;
    for (const [status, label] of Object.entries(labels)) {
      const button = document.createElement('button'); button.type = 'button'; button.dataset.status = status;
      const total = state.synths.filter(row => status === 'all' || row.status === status).length;
      button.textContent = `${label} (${total})`;
      button.addEventListener('click', () => { state.status = status; render(); }); filters.append(button);
    }
    render();
  } catch (error) {
    body.replaceChildren(); count.textContent = 'The filter could not load. Use the full compatibility list.';
    document.getElementById('synthSearch').disabled = true; console.error(error);
  }
}
load();
