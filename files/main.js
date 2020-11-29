function nav_collapse(x) {
  for (const ul of (x||document).querySelectorAll(
    x ? ':scope > ul' : '#nav li.click ul'
  )) ul.classList.remove('show');
}

window.onclick = function(e) {
  const t = e.target;
  if (t.classList.contains('collapse')) {
    nav_collapse();
  } else {
    for (const x of document.querySelectorAll('#nav li.click')) {
      if (x==t) {
        for (const ul of x.querySelectorAll(':scope > ul'))
          ul.classList.toggle('show');
      } else if (!x.contains(t)) {
        nav_collapse(x);
      }
    }
  }
}

window.addEventListener('keydown', function(e) {
  if ((e.which || e.keyCode)===27)
    nav_collapse();
});

document.addEventListener('DOMContentLoaded', () => {
  const regbtn = document.getElementById('regbtn');
  regbtn.onclick = function(e) {
    for (const x of document.querySelectorAll('#main > *'))
      x.style.display = x.id==='regform' ? 'initial' : 'none';
  };
});
