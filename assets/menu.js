// Show or hide the navigation links when clicking the "hamburger menu"
function menu_toggle() {
    var x = document.getElementById("sidebar-links");
    if (x.style.display === "block") {
        x.style.display = "none";
    } else {
        x.style.display = "block";
    }
}

// Hide the navigation links when clicking outside the sidebar
function menu_hide() {
    var x = document.getElementById("sidebar-links");
    if (x.style.display === "block") {
        x.style.display = "none";
    }
}

// Show the navigation links when the window is resized past the width of 768px
function menu_show() {
    var x = document.getElementById("sidebar-links");
    if (window.innerWidth <= 768) {
        x.style.display = "none";
    } else {
        x.style.display = "block";
    }
}

window.addEventListener("resize", function () {
    menu_show();
});
