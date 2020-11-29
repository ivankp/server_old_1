window.onclick = function(e) {
  const t = e.target;
  for (const x of document.querySelectorAll('#nav li.click')) {
    if (x==t) {
      for (const ul of x.querySelectorAll(':scope > ul'))
        ul.classList.toggle('show');
    } else if (!x.contains(t)) {
      for (const ul of x.querySelectorAll(':scope > ul'))
        ul.classList.remove('show');
    }
  }
}

window.addEventListener('keydown', function(e) {
  if ((e.which || e.keyCode)===27)
    for (const ul of document.querySelectorAll('#nav li.click ul'))
      ul.classList.remove('show');
});
