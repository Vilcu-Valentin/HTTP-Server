window.onload = function() {
    let favorites = JSON.parse(localStorage.getItem('favorites')) || [];
    document.querySelectorAll('figure').forEach(function(figure) {
        let img = figure.querySelector('img');
        if (favorites.includes(img.src)) {
            drawStar(img);
        }
        figure.addEventListener('click', function() {
            if (favorites.includes(img.src)) {
                favorites = favorites.filter(function(src) {
                    return src !== img.src;
                });
                clearStar(img);
            } else {
                favorites.push(img.src);
                drawStar(img);
            }
            localStorage.setItem('favorites', JSON.stringify(favorites));
        });
    });

    function drawStar(img) {
        let canvas = img.parentElement.querySelector('canvas');
        let ctx = canvas.getContext('2d');

        ctx.shadowColor = 'rgba(0, 0, 0, 0.8)';
        ctx.shadowBlur = 5;
        ctx.shadowOffsetX = 3;
        ctx.shadowOffsetY = 3;
        
        ctx.beginPath();
        ctx.moveTo(75, 10);
        ctx.lineTo(87,40);
        ctx.lineTo(120,40);
        ctx.lineTo(98, 70);
        ctx.lineTo(110,100);
        ctx.lineTo(75, 80);
        ctx.lineTo(40,100);
        ctx.lineTo(52, 70);
        ctx.lineTo(30,40);
        ctx.lineTo(63,40);
        ctx.lineTo(75, 10);
        ctx.fillStyle = "gold";
        ctx.fill();
    }
    function clearStar(img) {
        let canvas = img.parentElement.querySelector('canvas');
        let context = canvas.getContext('2d');
        context.clearRect(0, 0, canvas.width, canvas.height);
    }
}
