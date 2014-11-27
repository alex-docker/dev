var path = require('path'),
    config;

config = {
    development: {
        // The url to use when providing links to the site, E.g. in RSS and email.
        url: 'http://my-ghost-blog.com',

        // Example mail config
        // Visit http://docs.ghost.org/mail for instructions
        // ```
        //  mail: {
        //      transport: 'SMTP',
        //      options: {
        //          service: 'Mailgun',
        //          auth: {
        //              user: '', // mailgun username
        //              pass: ''  // mailgun password
        //          }
        //      }
        //  },
        // ```

        database: {
            client: 'sqlite3',
            connection: {
                filename: path.join(__dirname, '/content/data/ghost.db')
            },
            debug: false
        },
        // Don't modify these or update Dockerfile also
        server: {
            host: '0.0.0.0',
            port: '2368'
        }
    }
};

// Export config
module.exports = config;
