const MAX_TRIES = 10;
const MAX_LENGTH = 433;


function sleep(t) {
  return new Promise(res=>{
    setTimeout(res, t);
  });
}

let running = false;

function done() {
  running = false;
  let b = document.getElementById("run");
  let o_box = document.getElementById("output");
  b.classList.remove('is-loading')
  o_box.parentElement.classList.remove('is-loading')
  b.disabled = false;
}


async function run_code() {
  if (running) return;
  running = false;

  let b = document.getElementById("run");
  let o_box = document.getElementById("output");

  b.classList.add('is-loading')
  o_box.parentElement.classList.add('is-loading')
  b.disabled = true;

  let code = document.getElementById("code").value.trim();
  let ticket = document.getElementById("ticket").value.trim();
  console.log(code, ticket);

  localStorage.setItem('code',code);
  localStorage.setItem('ticket',ticket);
  o_box.value = 'Running...';
  o_box.style.color = '#868686';

  let res = await fetch('/jit/run', {
    method:'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      code: code,
      ticket: ticket,
    }),
  });
  let res_j = await res.json();
  if (!res_j.success) {
    o_box.value = res_j.error || 'Unkown error running code';
    o_box.style.color = 'red';
    done();
    return;
  }

  const runid = res_j.runid;

  for (let i=0; i<MAX_TRIES; i++) {
    await sleep(5000);
    let res = await fetch(`/jit/output?runid=${runid}`);
    let res_j = await res.json();
    console.log(res_j);

    if (res_j.output !== null) {
      o_box.value = res_j.output;
      o_box.style.color = 'black';
    }

    if (res_j.waiting === false)
      break;

    if (i == MAX_TRIES - 1) {
      o_box.value = 'Unkown error running code';
      o_box.style.color = 'red';
    }
  }
  done();
}

let code = document.getElementById('code');
let bar = document.getElementById('bar');
function update_bar() {
  let v = code.value;
  let p = Math.floor(100 * v.length / MAX_LENGTH);
  bar.setAttribute('value',p);
  if (p > 90) {
    bar.classList.add('is-danger');
    bar.classList.remove('is-warning');
    bar.classList.remove('is-success');
  } else if (p > 75) {
    bar.classList.remove('is-danger');
    bar.classList.add('is-warning');
    bar.classList.remove('is-success');
  } else {
    bar.classList.remove('is-danger');
    bar.classList.remove('is-warning');
    bar.classList.add('is-success');
  }
}
update_bar();

code.addEventListener('input',(e)=>{
  update_bar();
});

async function load_code() {
  let ticket = await localStorage.getItem('ticket');
  if (ticket) {
    document.getElementById("ticket").value = ticket;
  }
  let last = await localStorage.getItem('code');
  if (last) {
    document.getElementById("code").value = last;
    update_bar();
  }
}
load_code();
