/* Preserve the former Docsify guide routes; normal MkDocs anchors are untouched. */
(() => {
  const script = document.currentScript;
  if (!script) return;
  const docsRoot = new URL('../', script.src);
  const routes = {
    'Adaptation Programming Guide': 'programming-guide/',
    'Adaptation Testing Guide': 'testing-guide/',
    'README': '',
    '': ''
  };
  function redirectLegacyRoute() {
    if (!location.hash.startsWith('#/')) return;
    let route;
    try { route = decodeURIComponent(location.hash.slice(2)).split('?')[0].replace(/\.md$/, ''); }
    catch { return; }
    if (Object.hasOwn(routes, route)) location.replace(new URL(routes[route], docsRoot));
  }
  redirectLegacyRoute();
  addEventListener('hashchange', redirectLegacyRoute);
})();
