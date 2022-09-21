'use strict';

createBenchmark(main, {
  n: [ 10_000 ],
});

function main({ n }, bench) {
  bench.start();
  for (let i = 0; i < n; i++) {
    new URL('data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoicmVwb3J0LmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsiLi4vcmVwb3J0LnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLFlBQVksQ0FBQztBQUViLFNBQVMsSUFBSTtJQUNYLE1BQU0sQ0FBQyxHQUFHLElBQUksS0FBSyxDQUFDLEtBQUssQ0FBQyxDQUFDO0lBQzNCLENBQUMsQ0FBQyxNQUFNLENBQUMsR0FBRyxRQUFRLENBQUM7SUFDckIsTUFBTSxDQUFDLENBQUM7QUFDVixDQUFDO0FBRUQsSUFBSSxFQUFFLENBQUMifQ==');
  }
  bench.end(n);
}
